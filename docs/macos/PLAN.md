# TWASE macOS port – plan

Status: 2026-10-03. Phases 0–3 done: the dylib injects, patches, captures the Lua log, and the in-game Lua console works in the frontend and in campaign. Phase 4 (mod loader) and Phase 5 (diplomacy tweak, unit-size patch) work. Phase 6: CI and README done, waiting for the first CI run and a release tag. Target build: Feral *Total War: ATTILA* 1.6.1 RC2 (`CFBundleVersion 480285.103778`, Steam).

## 1. What we are porting *to* (findings)

| | Windows (TWASE today) | macOS (Feral port) |
|---|---|---|
| Architecture | **32-bit x86** (`set_arch("x86")`) | **arm64 only** (thin Mach-O, no x86_64 slice, so no Rosetta) |
| Where game code lives | `empire.retail.dll` (loaded later, watcher thread waits for it) | Statically linked into `Total War ATTILA.app/Contents/MacOS/Total War ATTILA` (100 MB) |
| Symbols | none (IDA naming by hand) | **Partially stripped: ~9,200 named functions** (mostly Lua script bindings like `EMPIREBATTLE::BATTLE_SCRIPT::BATTLE_SCRIPT_GLOBAL::*(lua_State*)`, event-feed templates, Feral/Steam glue). The `lua_*` API and our hook targets are unnamed, but the named bindings call straight into the Lua API, so they make great anchors. RTTI / mangled type names are also present (`N2CA…`, `UTILITYDLL3LUA5State`, `DIPLOMACY_LIKELIHOOD`) |
| Build ID | – | `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326` |
| File system | CA VFS | CA VFS **plus Feral's own `FeralVFS_*` layer**, so check that mod-file lookups still go through the CA VFS |
| Calling conventions | `__thiscall` / `__cdecl` / `__fastcall`+edx trick | One ABI (Apple AAPCS64): `this` in `x0`, args in `x0–x7`. Simpler |
| Pointer size | 4 | 8, so **every struct offset changes**, not just function addresses |
| Wide strings | `wchar_t` = UTF-16 | Game uses `char16_t` (`CHAR_TRAITSIDs`). macOS `wchar_t` is 4 bytes, so `WString` must use `char16_t` |
| Renderer | D3D11 (`IDXGISwapChain::Present`) | Metal (`CAMetalLayer`, `presentDrawable`) behind Feral's own layer |
| Input | WndProc + DirectInput8 | AppKit (`CFeralNSApplication`) / possibly GameController/HID |
| Lua | 5.1, separate exports found by RVA | 5.1 statically linked and stripped (`Lua 5.1` and `decoda_name` strings present) |

Code signing (it decides how we inject and hook):

- Developer ID, notarized, **hardened runtime** on.
- Entitlements that work for us:
  - `com.apple.security.cs.allow-dyld-environment-variables`: `DYLD_INSERT_LIBRARIES` is honoured.
  - `com.apple.security.cs.disable-library-validation`: our own unsigned or ad-hoc-signed dylib can load.
  - `com.apple.security.cs.allow-unsigned-executable-memory`: we can allocate RWX memory for trampolines.
- Missing: `cs.disable-executable-page-protection`. Patching signed `__TEXT` pages (inline hooks, the unit-size byte patch) **may get the process killed**. This has to be checked first (Phase 0).

Tooling notes:

- ⚠️ `TotalWarAttilaData/attila.dll` (the file your `.bndb` was made from) is a **resource-only Windows DLL**: icons, version info, and the Games Explorer XML pointing at `Rome2.exe`. It contains no game code. Open the Mach-O `Total War ATTILA.app/Contents/MacOS/Total War ATTILA` in Binary Ninja instead.
- Binary Ninja installed: **6.0 Free**. The GUI MCP server is included in every GUI edition (enable `ui.mcp.enabled`, then *Plugins ▸ MCP ▸ Start Server*, which serves `http://127.0.0.1:24642/mcp`). Despite the docs saying "read-only", the 6.0 server also has **mutation tools**: `bn_symbol_rename`, `bn_comment_set`, `bn_type_struct_create`, `bn_function_prototype_set`, and `bn_open_item_save`. So the bndb can carry names, types, and comments. `docs/addresses.md` stays the reviewed source for the C++ offset tables.
- `IDA Home (PC)` is the x86/x64 edition, so it cannot do arm64. Keep using it for the *PC* side of the cross-reference.
- Xcode license accepted (Apple clang 21); `nm`/`otool`/`python3` work.

## 2. Language: C++ or Rust?

**Recommendation: stay with C++23, using Objective-C++ (`.mm`) for the platform layer, and keep xmake.**

Why:

1. **Hooking library on arm64.** In C++ there are mature options: Dobby (small, arm64/macOS) or frida-gum. In Rust, `retour` is x86-only. That leaves the `frida-gum` bindings (a heavy C devkit underneath) or writing an arm64 trampoline/relocator yourself. That means relocating `ADRP`/`ADR`/`B`/`BL`/`LDR literal`/`CBZ`/`TBZ` in the prologue.
2. **Platform glue is Objective-C.** Metal overlay, `NSEvent` monitors, method swizzling, and `NSAlert` are trivial in Objective-C++. Rust's `objc2` works, but every call is an `unsafe` FFI hop.
3. **ImGui.** `imgui_impl_metal` + `imgui_impl_osx` are first-party C++/ObjC backends. In Rust, `imgui-rs` lags upstream and has no maintained Metal backend.
4. **Reuse.** About 40% of the PC code ports as-is or nearly as-is: Config (toml11), logging (spdlog), LuaConsole, Commands, TweaksTab, the mod-loader logic, and SemVer. The same language also lets the two repos later share a common core.
5. **Rust's safety pays off little here.** Most of the code is raw calls into game memory with hand-derived layouts, so it would be `unsafe` anyway.

When Rust *would* make sense: if you want to learn it, or if the extender later grows a large amount of game-independent logic. Even then, the Rust-for-core, C/ObjC-for-hooks split adds a second toolchain for little gain right now.

## 3. Component mapping (PC → macOS)

| PC component | macOS replacement | Reuse |
|---|---|---|
| `loader/` winmm.dll proxy (1.5k lines) | Not needed. Inject with `DYLD_INSERT_LIBRARIES` from a launcher (script or a tiny CLI), or set it in Steam launch options if Steam passes env through (spike 0.3). Fallback: proxy `libsteam_api.dylib`, but that edits the bundle, needs *App Management* permission, and Steam "verify files" reverts it | ❌ |
| `DllMain` + watcher thread waiting for `empire.retail.dll` | `__attribute__((constructor))` in `libTWASE.dylib`. The game code is already mapped, so hooks can be installed before `main()`. ASLR slide comes from `_dyld_get_image_vmaddr_slide(0)` (image base `0x100000000`) | rewrite (small) |
| Detours + `DetourTransaction` + `Hook<T>` | `Hook<T>` over Dobby (or frida-gum), with offsets as `uint64_t` from the image base. Drop the `edx` dummy parameters | adapt |
| `Patches` (`PatchBytes` with expected-bytes check) | Same idea, but the write goes through `mach_vm_protect(VM_PROT_COPY)` and the bytes are arm64 (e.g. retarget a `B.HS`) | adapt |
| `Image` / `FileVer` (PE version resource) | Read the `CFBundleShortVersionString`/`CFBundleVersion` and **`LC_UUID`** of the main image, then pick the offset table by UUID (exact-build match) | rewrite |
| `Paths` (wide strings, module path) | `TWASE/` folder in the game root next to the `.app`: `…/common/Total War Attila/TWASE/{config.ini,logs}`. Never inside the `.app`, because that breaks the seal | adapt |
| `Utils` (MessageBox, Widen/Narrow) | `NSAlert`/`CFUserNotificationDisplayAlert`. Narrow-only strings; char16_t helpers for game strings | adapt |
| `Config`, spdlog setup, `SemVer` | as-is | ✅ |
| `D3D11Hook` (Present + DX11/Win32 ImGui) | `MetalHook.mm`: swizzle `-[MTLCommandBuffer presentDrawable:]` / `-[CAMetalLayer nextDrawable]`, render ImGui with `imgui_impl_metal` into the drawable. ObjC swizzling **does not touch `__TEXT`**, so it is unaffected by code signing | rewrite |
| WndProc subclass + `DInput8Hook` | `InputHook.mm`: `[NSEvent addLocalMonitorForEventsMatchingMask:]` (returning `nil` swallows the event) + `imgui_impl_osx`. Console key below Esc: `kVK_ISO_Section` (0x0A, ISO/German) and `kVK_ANSI_Grave` (0x32, ANSI). If Feral reads keys via GameController/IOHID instead, hook that too (spike 0.6) | rewrite |
| `UI/LuaConsole`, `Commands`, `TweaksTab` | as-is (ImGui) | ✅ |
| `SetLuaLogger` (LuaLog hook) | re-find, same logic | logic ✅ |
| `RunStartupPath` mod loader | re-find hook + VFS/CName/tw_free; re-derive `ScriptingEnv` layout; check the path separator (`mods\\` vs `mods/`) on the Mac VFS | logic ✅ |
| `DiplomacyLikelihood` | re-find 3 hooks + `UIComponent_SetTooltipText`, `WString` ctor/dtor; re-derive UI offsets; `wchar_t`→`char16_t`; `swprintf_s`→own formatting | logic ✅ |
| `LogMods` (`mod_list.txt` from the command line) | Feral's launcher manages mods differently. Find where it writes the mod list (likely under `~/Library/Application Support/Feral Interactive/Total War ATTILA/`) | rewrite |
| sdk `Addresses.hpp`, `Lua/Addresses.hpp` | `sdk/Attila/Offsets_<uuid>.hpp`, all re-found | ❌ |
| sdk structs (`ScriptRuntime`, `ScriptInterface`, `RuntimeLuaNode`, `ScriptingEnv`, `WString`, `TempString`, `VFSSearchResults`, UI offsets) | re-derive for 64-bit; add `static_assert(offsetof(...))` for every field we use | ❌ |

## 4. Finding the addresses (Binary Ninja + MCP)

There are 30 addresses to re-find (13 game functions/globals, 12 Lua API functions, and a handful of struct offsets), plus the patch site. You can't diff x86→arm64 automatically with your licences, so match by **anchors**:

1. **For each PC address**, write down from your IDA PC database:
   - the strings it or its direct callers/callees reference
   - distinctive immediates (e.g. `0x40`, tweaker values ±4)
   - its vtable/RTTI class, if it is virtual
   - its call-graph shape (number of callees, which ones have strings)
2. **In Binary Ninja**, locate the same anchors in the Mach-O. Claude can drive this through the MCP server: strings → xrefs → decompile → compare against the PC pseudo-C.
3. **Record every hit** in `docs/addresses.md`: name, PC RVA, Mac offset, anchor used, confidence, and notes such as "inlined at N sites".

Specific strategies:

- **Lua API (12 functions).** Quickest route: decompile a few of the ~600 *named* script bindings (`BATTLE_SCRIPT_GLOBAL::set_volume(lua_State*)` etc.). Their callees are `lua_gettop`/`lua_tolstring`/`lua_settop`/… Cross-check with `lbaselib`'s `luaL_Reg` table is pairs of name string + function pointer in `__DATA_CONST`. From the `"pcall"`, `"next"`, `"type"`, `"getmetatable"`, `"tostring"`, and `"loadstring"` strings you get `luaB_*`, and those call straight into `lua_pcall`, `lua_next`, `lua_type`, `lua_getmetatable`, `lua_settop`, `lua_pushvalue`, `lua_pushnil`, `lua_tolstring`, `lua_gettop`, and `luaL_loadbuffer`. `lua_getfield` and `lua_remove` come out of `lauxlib` (`luaL_getmetafield`, `luaL_findtable`). Optionally build stock Lua 5.1.5 with Apple clang `-O2 -arch arm64` and compare shapes side by side.
- **Script interfaces / runtime list.** Start from the `"decoda_name"` xref and the RTTI names around `UTILITYDLL::LUA::State`.
- **Mod loader.** Start from the `"scripting.lua"` string to reach `RunStartupPath`, then the VFS calls inside it.
- **Diplomacy.** Start from the `"dy_chance"` string and the `DIPLOMACY_LIKELIHOOD` mangled signatures. Expect clang to inline small helpers more aggressively than MSVC did.
- **Unit-size crash.** ✅ Found via the libc++ string `"bitset test argument out of range"`, see `docs/addresses.md`.
- **Small functions (`GetLuaState`, `WString` ctor/dtor, `CName` ctor).** If clang inlined them away, reimplement them in our code instead of calling them.

Feral updates are rare. **Hardcoded offset tables keyed by `LC_UUID` are enough for now.** If the UUID doesn't match, TWASE logs it and disables hooks instead of crashing. Pattern scanning can come later if needed.

## 5. Phases

### Phase 0: environment + feasibility spikes (do these first; they can change the design)

- [x] 0.1 `sudo xcodebuild -license accept`
- [x] 0.1b `brew install xmake` (v3.1.1)
- [x] 0.2 Mach-O opened in Binary Ninja, bndb at `<game root>/Total War ATTILA/Total War ATTILA.bndb`, MCP server up (view `view_1`, base `0x100000000`).
- [ ] 0.3 **Injection.** A hello-world dylib whose constructor writes a log line (`spikes/inject`). Test:
  - (a) ✅ Running the binary directly with `DYLD_INSERT_LIBRARIES` while Steam is running works. The dylib loads (ad-hoc signed, from outside the bundle), there is no Steam relaunch, and `SteamAPI_Init` is OK. Code-signing flags in the process: `VALID HARD KILL RUNTIME`. Note: with injection, dyld image 0 is *our* dylib; find the game by `MH_EXECUTE`.
  - (b) Steam launch options `DYLD_INSERT_LIBRARIES=… %command%`
  - (c) ✅ launcher script `scripts/twase-launch.command`

  Watch for macOS stripping `DYLD_*` when a SIP-protected binary such as `/bin/sh` sits in the chain.
- [x] 0.4 **Code patching under the hardened runtime.** ✅ Works without re-signing. `spikes/inject` rewrites the page holding the entry point (`main`, executed right after the constructors). Both methods ran without a kill or crash report (3/3 runs):
  1. `mach_vm_protect(RW|VM_PROT_COPY)` → write → `RX` + `sys_icache_invalidate`
  2. copy the page to `mmap` anonymous memory → `RX` → `mach_vm_remap(VM_FLAGS_OVERWRITE)` over the original (relies on `allow-unsigned-executable-memory`)

  `Memory.cpp` uses method 2 by default (the remap swaps the page atomically, so other threads never see it non-executable) and falls back to method 1. ✅ A Dobby inline hook on `luaL_loadbuffer` works in the real game (22 loads traced in the frontend).

  The two runs that ended before the 30 s mark were quit manually. All runs started cleanly.
- [x] 0.5 **Metal overlay.** ✅ Feral presents with `-[MTLCommandBuffer presentDrawable:]` (concrete class `AGXG16XFamilyCommandBuffer` on M4 Pro) on a render thread. TWASE swizzles it, found via `-[CAMetalLayer nextDrawable]`, and encodes ImGui into the drawable on the game's command buffer. No code patching involved.
- [x] 0.6 **Input.** ✅ An `NSEvent` local monitor sees keys and mouse (only app/system events go through `sendEvent:`). Console key `kVK_ISO_Section` (0x0A) on ISO keyboards, `kVK_ANSI_Grave` on ANSI. Events are queued on the main thread and fed to ImGui on the render thread.

### Phase 1: skeleton

- [x] `xmake.lua`: `libTWASE.dylib` (C++23), arm64, packages spdlog/fmt/toml11/dobby; post-build copy of the dylib to `TWASE/` and the launcher to the game root. Links Foundation so dyld initializes it before our constructor.
- [x] Port `Config`, `Paths`, logging, `Utils`; Mach-O `Image` with an `LC_UUID` check (hooks disabled on unknown builds); `Hook<T>` on Dobby + `Memory::PatchBytes`. (`SemVer` not needed yet.)
  - Lesson: never call `CFBundleGetValueForInfoDictionaryKey` (or anything else that goes through Foundation's localization) from the constructor. It recursed forever before Foundation was initialized.
- [x] Launcher `scripts/twase-launch.command` (deployed to the game root).
- [ ] Install instructions, including `xattr -dr com.apple.quarantine` and ad-hoc `codesign -s -` for downloaded builds (Phase 6 README).

### Phase 2: Lua core

- [x] Find the 12 Lua API functions, LuaLog, `g_RuntimeLuaList{Head,Sentinel}`, and `GetLuaState`; re-derive `ScriptRuntime` / `RuntimeLuaNode` (see `docs/addresses.md`). Watch out: the binary has two Lua copies, and the game uses the one at `0x103f…`.
- [x] LuaLog: a data hook on the sink pointer + a `nop` in its setter. Lua log lines show up in `TWASE/logs`.
- [x] Port `LuaRuntime` and `LuaGameEnvironment` (`Init` takes the ASLR slide).

### Phase 3: console

- [x] Metal + input hooks; port `LuaConsole` and `Commands`. Works in frontend and campaign (battle not tested yet).
- [x] **Threading (differs from PC):** game Lua runs on the `WinMain` thread, the overlay on the render thread. Console commands and the context list are queued and run in a hook on the per-frame `GameTick` (`0x1010ecd24`). The overlay only reads cached results.
- [x] `TweaksTab` with the diplomacy toggle (saved to `config.ini`).

### Phase 4: mod loader

- [x] RunStartupPath, `VFS_GetInstance`, `VFS_SearchFiles` (a VFS virtual), `CName_ctor`, `tw_free`, and the 64-bit `ScriptingEnv`/`TempString`/`VFSSearchResults` layouts, cross-checked against RTTI (see `docs/addresses.md`). ✅ A loose `TotalWarAttilaData/data/campaigns/main_attila/mods/<name>/scripting.lua` auto-loads; Lua errors in a mod are caught and logged.
- [x] Mods inside a `.pack` load too. Feral's user `maps` folder is not usable (not searched, and wiped on every launch).
- [ ] Document the local mod locations in the README (Phase 6).

### Phase 5: tweaks + patches

- [x] Diplomacy deal-score tooltip: all functions and offsets found (see `docs/addresses.md`), tested in the game. Lesson: hooks must not use `App::Get()` from the `App` constructor, hooking now runs in `App::Initialize()`.
- [x] Unit-size patch: the same bitset<64> throw exists on Mac. TWASE applies the one-instruction `b.eq` retarget at `0x101a51c58`. 2026-10-04: a Fireforged Empire save loads and plays fine with the patch (whether the >64 case was hit isn't logged).
- [x] Mod list logging: the `WinMain` hook logs the engine command line and the enabled mods.
- [ ] Start the game with mods from the launcher script (to test the unit-size patch with Fireforged). Findings so far (2026-10-03):
  - Feral's preferences (`~/Library/Application Support/Feral Interactive/Total War ATTILA/Preferences Data`, XML) have `DisableAllMods = 1` and a `mods` list. Loading a Fireforged save with mods disabled crashes on a missing DB record (not TWASE).
  - On Windows, Runcher writes `mod_list.txt` (`mod "x.pack";`, `add_working_directory "…";`) and starts `Attila.exe mod_list.txt;` (+ `game_startup_mode campaign_load <save>`). It doesn't support macOS.
  - 2026-10-04: Feral builds the command line itself (`0x100022b7c`, mods via `0x100014f1c`). Process arguments don't reach the engine; Feral's pre-launcher shows on every start unless `GameOptionsDialogShouldShow = 0`. ✅ `ExtraCommandLineEnabled` + `ExtraCommandLine` in Feral's preferences are appended to the engine command line, so a direct start with a chosen mod set needs only preference edits (see `docs/addresses.md`). ✅ `twase-launch.command --skip-launcher | --mods <Runcher mod_list.txt> | --load <save> | --extra <statements>` sets these preferences for one launch and restores the changed keys afterwards. With `DisableAllMods`, Feral empties its own mods folder, so the launcher links missing packs from the Workshop folder into `TotalWarAttilaData/data` and removes the links on exit. Tested: 6 mods + `--load` of a Fireforged save go straight into the campaign.
  - The Mac binary still has CA's command-line keywords as UTF-16 strings (`add_working_directory`, `game_startup_mode`, `campaign_load`). Feral's game shell (`0x100f858f0`) starts the `WinMain` thread through `0x100020344`, which converts a string into the UTF-16 command line for `WinMain` (`0x1032bc10c`). Still to check: where that string comes from (`argv`?) and how Feral maps paths (Lua paths show a virtual `q:\feral\users\default\...` drive), then try `mod_list.txt;` as an argument.

### Phase 6: release

- [x] GitHub Actions on `macos-15` (arm64): build, ad-hoc sign, zip `TWASE/libTWASE.dylib` + launcher; nightly on `main`, versioned release on `v*` tags (tag must match `TWASE_VERSION_STR`), as on PC. Packaging dry-run passed locally.
- [x] Steam integration (2026-10-05): `twase-launch.command` works as a Steam launch option (`"<game folder>/twase-launch.command" %command%`). Steam passes the `.app` for `%command%`; macOS strips `DYLD_*` on the way through `/bin/sh`, so the launcher takes Steam's libraries (steamloader, overlay) from `STEAM_DYLD_INSERT_LIBRARIES` and adds TWASE after them. Tested: console and Steam overlay both work. Lesson: overwriting a dylib in place (same inode) makes the kernel kill the game with "Code Signature Invalid / Invalid Page" (cached signature); the xmake deploy now copies to a temp file and renames it.
- [x] README for players: install, quarantine removal, launcher options, console, mod locations, config, troubleshooting.
  - Tested: a quarantined (downloaded) dylib is refused ("library load disallowed by system policy") and the game aborts. The launcher now removes the quarantine from `TWASE/` itself; the README has a one-time `xattr` command for the launcher script.
- [x] First CI run on GitHub (green, nightly published), released as `v1.0.0` (own versioning, feature parity with TWASE v1.0.0 for Windows).

## 6. Proposed repo layout

```text
twase_macos/
  xmake.lua
  PLAN.md
  docs/addresses.md          # PC RVA ↔ Mac offset ↔ anchor, per LC_UUID
  scripts/twase-launch.command
  src/
    dylib/
      main.mm                # constructor/destructor
      App.*  Config.*  Paths.*  Utils.*  Image.*  SemVer*
      Hooking/  Hook.hpp  Memory.cpp
      Hooks/    LuaLog.cpp  RunStartupPath.cpp  DiplomacyLikelihood.cpp  MetalHook.mm  InputHook.mm
      Patches/
      UI/       LuaConsole.cpp  Commands.cpp  TweaksTab.cpp
    sdk/Attila/              # 64-bit layouts + Offsets.hpp (keyed by LC_UUID)
```

Later option: move the platform-neutral parts (Config, UI, Commands, mod-loader logic, sdk interfaces) into a shared core used by both TWASE and twase_macos.

## 7. Top risks

1. ~~Hardened runtime kills `__TEXT` patches~~. Ruled out by 0.4. If a future Feral build drops `allow-unsigned-executable-memory`, fall back to an ad-hoc re-sign with `disable-executable-page-protection`.
2. ~~Steam relaunch drops `DYLD_INSERT_LIBRARIES`~~. No relaunch when started directly with Steam running (0.3a). Steam launch options (0.3b) are still untested.
3. **Feral changed or inlined code.** Some PC hook points may not exist one-to-one (clang inlining, Feral's own renderer/input/filesystem/logging layers). Each hook may need a different but equivalent site.
4. **Layout mistakes from the 32→64-bit change.** Mitigated with `static_assert`s and verifying every struct in the debugger (`lldb` attach works only if the app is re-signed with `get-task-allow`; otherwise log-based checks).

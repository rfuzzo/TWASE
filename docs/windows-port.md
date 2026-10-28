# Windows port of the Mac features (plan)

Status 2026-10-13: Windows is at the v1.0.0 features (console, Lua log, mod loader, unit size patch, deal score tweak), plus the shared core from `src/core` (config, console with the command queue). The Mac build has more, all of it needing addresses and struct layouts from the game code, which on Windows is in `empire.retail.dll` (TWASE resolves its addresses relative to that DLL's base, `App::GetEmpireDllAddr()`). This plan is for the PC side (IDA Home, x86).

Progress: steps 1-6 are done, step 7 is in progress (branch `win/port-loose-mods-lua-api`). To port a `twase.*` function: add its addresses to `src/platform/win32/sdk/Attila/Addresses.hpp` under the Mac names, then set its flag in `src/platform/win32/sdk/Attila/Features.hpp` to 1. The engine's member functions are declared `TWASE_THISCALL` in `src/core/Scripting/LuaApi.cpp`; check each one's convention in IDA when porting it.

## First: test the shared core on Windows

PR #7 moved the config and the console into `src/core` and gave Windows the console's command queue (typed commands now run from the D3D11 Present hook, as before, but through `LuaConsole::ProcessPending()`). It was only compiled on Windows. Test in the game:

- the console opens with the key below `Esc`, Lua runs (`print(1)`), `.contexts` / `.switch` work
- the Tweaks tab toggles the deal score and saves `config.ini`
- a fresh `TWASE/config.ini` gets written (the `[dev]` section now has `trace_lua_loads` and `lua_dev_functions`)

## What's missing on Windows

| Feature | Mac source | Needs from `empire.retail.dll` |
|---|---|---|
| Loose mods next to a pack with the same `mods` folder | `Hooks/RunStartupPath.cpp` (`EnumerateModFolders`, the disk scan) | nothing, plain C++: copy it into the Windows hook |
| More of the game's Lua API (for `twase.*`) | `sdk/Attila/Lua/LuaRuntime.*`, `Lua/Addresses.hpp` | 11 functions: `lua_pushnumber`, `lua_tonumber`, `lua_pushstring`, `lua_pushboolean`, `lua_pushcclosure`, `lua_createtable`, `lua_setfield`, `lua_touserdata`, `luaL_error`, `lua_rawequal`, `lua_toboolean` |
| `twase` table, `twase.version` | `Scripting/LuaApi.cpp` (`Register`), called from `RunStartupPath` | the functions above |
| `twase.faction_attitude` | `LuaApi.cpp` | `FactionScriptInterface_GetArg`, `CAI_INTERFACE::get_faction_attitude`; offsets script interface → FACTION, FACTION → model handle → world → CAI interface, the alive marker |
| `twase.region_wealth`, `twase.region_province` | `LuaApi.cpp` | offsets REGION_SCRIPT_INTERFACE → REGION, REGION → wealth, wealth breakdown, `regions` record |
| `twase.building_level`, `twase.unit`, `twase.levy_pool` | `LuaApi.cpp` | the campaign global, campaign → handle, world → DB field, `DB_GetContainer`, the table accessors (7), `StringHashMap_Find`, DB table layout (index, records, count), the record layouts |
| `twase.localise` | `LuaApi.cpp` | DB container → localisation, its lookup's vtable offset, `CA::String` ctor / dtor |
| dev probes (`twase.dev_*`) | `LuaApi.cpp` | a safe memory read (Windows: `ReadProcessMemory` on the own process, or SEH) instead of `mach_vm_read_overwrite` |
| Region trading (`twase.enable_region_trading`) | `src/core/Hooks/NativeRegionTrading.cpp` | 84 addresses and offsets: the region action's rules, the trade regions panel, the "Add Offer/Demand" list, deal terms, the evaluator and its components, the transfer |

Every Mac address is in [docs/macos/addresses.md](macos/addresses.md) with how it was found (strings, RTTI, callers); many entries also have the Windows RVA from v1.0.0 for comparison. `src/platform/macos/sdk/Attila/Addresses.hpp` has them all by name.

## Order

Each step works and can be tested on its own.

1. **Loose mods** (no RE). Done.
2. **Lua API**: the 11 functions. Done: `lua_Number` is `float` on Windows too. They're Lua 5.1 functions, so they're easy to find through `luaopen_base`, the only code that references the base library's `luaL_Reg` table (`"print"`, `"tonumber"`, ...): `luaB_tonumber` calls `lua_tonumber` / `lua_pushnumber`, `luaopen_base` calls `lua_pushcclosure`, `lua_createtable`, `lua_setfield`. Check whether Windows' `lua_Number` is `float` too (`lua_pushnumber` storing a single precision value): on Mac it is, and the API depends on it.
3. **Move `LuaApi.cpp` to `src/core`** (done, per-platform `sdk/Attila/Features.hpp` flags), so both platforms share the `twase.*` code. What it needs from the platform: resolving an address (Mac: `Image::Get()->GetSlide()`, Windows: `empire.retail.dll`'s base, `App::GetEmpireDllAddr()`), the safe read for the dev probes, and the same names in each platform's `Addresses.hpp`. Functions whose addresses Windows doesn't have yet can stay unregistered there (one `#if` or an `available` flag per function).
4. **`twase.faction_attitude`** (done: `FactionScriptInterface_GetArg` 0x80C480, `get_faction_attitude` 0xD38000 returns a float there): `FactionScriptInterface_GetArg` (the faction argument check of methods like `allied_with`) and `CAI_INTERFACE::get_faction_attitude`. On Mac the latter was found through the RTTI of its lambda (`…CAI_INTERFACE20get_faction_attitude…$_0`); MSVC names lambdas `<lambda_…>` without the function, so on Windows go through its callers instead: the diplomacy screen's attitude display, which buckets the value into 5 attitude levels.
5. **Regions and the database**: offsets change with the pointer size (a `CA::String` is 12 bytes on x86, 16 on arm64), so every record layout has to be found again: turn on `lua_dev_functions` and use `twase.dev_db_fields(table, index)` to dump records, then match them with the TSV export of the table (`twpack db export`).
   Done (2026-10-22): `twase.region_wealth` (Olisipo 373 = farming 33 + fertility_agriculture 164 + sea_trade 176, like on Mac), the DB tables, and the record layouts from `twase.dev_db_fields` dumps checked against the TSV export (all in `src/platform/win32/sdk/Attila/Addresses.hpp`).
6. **`twase.localise`**. Done: the loc manager is at DB container + 0x1694, its lookup is virtual + 0x0 (Mac: + 0x30).
7. **Region trading**, last and largest. Port it in the order of the Mac commits (`git log --follow src/core/Hooks/NativeRegionTrading.cpp`): rules → panel → evaluation → Region for Troops. On x86 the engine's functions are mostly `__thiscall`, and struct returns go through a hidden pointer instead of arm64's `x8`.
   The logic is now shared in `src/core/Hooks/NativeRegionTrading.cpp`; each platform has only its hooks in `Hooks/NativeRegionTradingDetours.cpp` (thiscall functions as fastcall shims on Windows, the hidden pointer for the region list). Ported by area, each one testable:
   - rules, evaluation and transfer (together, so the AI never gets regions for free): done, tested 2026-10-22 with region_trading.pack (its loc names the action "Trade Regions"). Vanilla's panel then shows only the offers, one region per deal.
   - the panel (both lists, several regions, OK, the deal list labels): done, tested 2026-10-22. Windows differences: the click handler runs on the panel's listener (panel + 0x30), `UIComponent_FindChild` is cdecl, the virtual Resize takes (width, height, bool, bool) (`Engine::Resize` in the hooks file), and the "deal" the OK button's `DiplomacyContext_SetRegion` applies the terms to is the trade regions action itself.
   - Region for Troops (`TWASE_HAS_REGION_FOR_TROOPS`): done, tested 2026-10-22. The "Add Offer/Demand" entries are 0x14 bytes on x86 and their vector's push is inlined (`Engine::PushActionEntry` rebuilds it with the engine's allocator and WString copy), the action button's click handler runs on the dropdown + 0x40 and has no thunk.
   - the debug log's breakdown of the gift score: done, the gift component's score parts are Mac's vtable slots - 0xc on Windows (`DealComponentGift_*Slot`).

## Finding things in `empire.retail.dll`

- **The campaign API is the same on both platforms**: same game scripts, same bindings. `tools/apigen/dump.lua` runs on Windows unchanged; diff its output with `tools/apigen/data/api_dump_macos.tsv` to confirm.
- **Lunar method tables** are plain pointer arrays in `.rdata` on Windows (no chained fixups): a name pointer and an MSVC member function pointer per entry (its size depends on the class's inheritance), and the method name strings lead straight to the handlers. The Mac handler addresses in `tools/apigen/data/lunar_types_macos.json` say which handler to compare.
- **Inlining differs**: MSVC and clang inline differently, so a function that's separate on Mac can be inlined on Windows (or the other way round). Then hook the caller, or patch at the call site.
- **Hooks**: Detours on Windows (`Hooking/DetourTransaction`), Dobby on Mac; the hook bodies are the same C++.

## Done when

- every `twase.*` function exists on Windows and gives the same results as on Mac (the tests in `docs/macos/addresses.md` list values to compare, e.g. Olisipo's wealth breakdown, `twase.region_province` for Olisipo → `att_prov_lusitania`)
- [lua-api.md](modding/lua-api.md) no longer says "Mac only"
- the region trading mod works on Windows

## Campaign UI controls (2026-10-28)

The encyclopedia URL and settlement-card slot controls use a separate Windows
backend in `src/platform/win32/dll/Hooks/CampaignUI.cpp`. It installs after the
normal hook transaction, before the console/input hooks. A failed resolution is
logged, leaves the normal hooks operational, and makes the shared Lua setters
report unavailable controls rather than accepting an ineffective setting.

### Binary evidence and resolution

The locally available Windows image is **PE32 / i386**, preferred base
`0x10000000`, PE timestamp `0x648F6D5D` (2023-06-18 20:47:25 UTC),
`SizeOfImage = 0x255F000`. The installed Steam copy is:

`~/Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/common/Total War Attila/empire.retail.dll`

SHA-256: `799297d5b3779bb291e2e348a11821c0d803baac1b6130393de35259768f9a60`.
An older loose copy at `~/at/Total War Attila/empire.retail.dll` has SHA-256
`981b162ea0bc2c029d93c5056d5b58e05d54c2c5a525becaf215d7b3b8e05ab7`;
the settlement initialization bytes are identical in both.

This layout **does not match the existing fixed-RVA table**: for example, that
table's `g_Campaign = 0x28D52B0` lies beyond this image. Therefore the new hooks
do not publish guessed current-build function RVAs. They resolve the loaded
`empire.retail.dll` using an independently implemented PE-section search and
require unique targets plus the following consumer/layout checks. All addresses
below are **RVAs of the analyzed 2023 image**, not assumed current-build RVAs:

| Target | RVA | Independent binary evidence |
|---|---:|---|
| Native URL string | `0x1B712CC` | NUL-terminated `http://Atenc.totalwar.com/#` in read-only data |
| URL pointer | `0x1CD0CD8` | Unique aligned writable pointer to that string |
| URL builders | `0xEC56F0`, `0xEC57A0` | `PUSH [pointer]` at `0xEC571A` / `0xEC57E5`, feeding string concatenation; exactly two such code consumers |
| Settlement callback initialize | `0x13E1D50` | ECX saved in ESI; one stack REGION pointer stored at callback `+0x40`; `building_slot_%d` formatting; unsigned loop compares callback `+0x48` at `0x13E2513` and `0x13E274C`; epilogue `RET 4` at `0x13E2765` |
| Callback construction | `0x13C2210` | Writes byte `+0x44 = 0`, bound `+0x48 = 4`; called from factory `0x13C2610` referencing `CampaignSettlementCallback` |
| Capital-card presentation | `0x13EAB70` | Writes byte `+0x44 = 1`, bound `+0x48 = 6`, returns without stack arguments |
| Province card population | `0x13E8510` | Separates `capital` and `settlement` UI templates, then calls initialize at `0x13E89C8` / `0x13E8A9A` |

The detour uses the existing TWASE `Hook` / Detours convention: a `__fastcall`
shim receives ECX as `this`, ignores EDX, and takes the REGION pointer as its
single stack argument. It forwards the EAX return value. The resolved function
address is converted to an RVA before constructing `Hook`; the existing helper
adds the actual DLL base.

The production resolver checks i386/PE32, section protections, the unique URL
string/pointer and both instruction consumers, and a separately recorded
initialization entry followed by the slot formatter reference, both bound
comparisons, and the stack-cleanup epilogue. It deliberately rejects an
unrecognized layout; it does not hook an arbitrary first pattern match. These
checks do not establish support for every future compiler/game revision.

### Slot semantics and reset

The `major`/`minor` split follows the game's **province-capital presentation
card**, not simply whichever settlement the player clicked. In population,
the selected settlement's virtual `+0x14` method supplies its REGION; the safe
pointer at REGION `+0x350` supplies the province. The code takes the key
(`REGION +0x54`) from the province's REGION pointer at `+0x4C`, compares it
against the province-region list (`+0x44` count, `+0x48` array), and sends the
matching region to the `capital` template and capital-card presentation setter.
Other regions use `settlement`. Selecting another region in the same province
therefore does not itself change which callback receives the major override.

Initialization only reads the max-slot field, and constructs/configures each
slot synchronously before returning. The detour saves the **actual incoming
engine bound**, substitutes the configured value during that call, then restores
the saved value. It does not assume a default of four or six when restoring.
Native initialization never visits children above its current bound, so simply
restoring the field is insufficient when a reused card previously displayed
more slots. Each card class therefore keeps a conservative managed high-water
mark (no callback-pointer tracking or heap allocations). After initialization,
existing children above the effective bound through the larger of the incoming
native bound and that mark are hidden.
This covers both decreasing and clearing an override, including lowering below
the native count. Missing custom-template children are harmless null lookups.
Native bounds above twenty are preserved, not truncated. A class for which no
override has ever been used forwards directly to the original function.

The cleanup's `FindChild` (cdecl, analyzed RVA `0x137A980`) and `SetVisible`
(thiscall, analyzed RVA `0x13911E0`) are recovered from calls in the resolved
initialization body and checked against their own entry instructions. They
never use the mismatched fixed-RVA table. Callback `+0x4` is the lookup root.
Slot names use a borrowed, stack-backed 12-byte `TempString`; the lookup's map
comparator (`0x273B70` → `0xDC360` → `0xDF7A0`) only reads length `+0` and data
`+8`, without copying, retaining, destroying, or allocating the key. The hook
does not invoke the engine's allocating string constructor or formatter.

The URL pointer is exchanged only after replacement storage has been allocated
and filled successfully. Nil/empty reset restores the original pointer. Detach
also restores the original pointer without freeing custom storage while an
engine reader might still hold it.

These findings came from static PE parsing and x86 disassembly with Capstone;
IDA was unavailable because its Python integration could not import
`ida_domain` / `idapro`. Upstream was consulted for behavioral/signature/field
facts only; no upstream source, assembly hook, or scanner implementation was
copied. No Windows build or in-game exercise was performed in this macOS worker
session. The current supported Windows DLL was not available locally, so its
runtime resolution, card refresh/reset, and encyclopedia navigation remain
checks for the Windows integration run.

Windows integration run (2026-10-28, Steam build): the loaded DLL resolved the
encyclopedia pointer at RVA `0x1D33D00` and settlement Initialize at
`0x1407530`. With a custom URL, building, unit, and technology encyclopedia
links opened the replacement page with the game's entry suffix in the in-game
`ui\Common UI\browser` panel (Steam HTML surface); resetting restored the native
site. The two URL builders (`0xECE730`, `0xECE7E0`) only apply the prefix to
keys found in the game's encyclopedia table, then call `0x1413AE0`, which uses
`ShellExecuteW` instead when `CW_COPA_GetNativePlatform()` is non-zero. Pages
need an explicit background and text colour: an unstyled page rendered black in
that panel.

Card slot overrides, as logged by `inspect()` on a main-campaign province:
3/1 showed 3 capital and 1 minor slot; 2/2 then shrank all cards to 2 with no
stale slots; reset restored the native 6/4. 8/6 stayed at 6/4 because the
vanilla layout has no further slot widgets. Desolate settlement cards ignore
the overrides. Buildings in hidden slots, including one built and one destroyed
that turn, carried over end turn and reappeared on reset. As documented,
overrides persist across save loads within one game process. Quitting to the
main menu and exiting caused no crash. Charlemagne and Last Roman campaigns are
untested.

# Integrity checks and the in-game quirks

Build: Feral 1.6.1 RC2 (Steam), `CFBundleVersion 480285.103778`, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326`.
Addresses are unslid VAs (image base `0x100000000`); runtime = VA + ASLR slide. Found with Binary Ninja over MCP and
Unicorn emulation, see [re-tooling.md](re-tooling.md). The functions below are named in the Steam database
(`<game root>/Total War ATTILA/Total War ATTILA.bndb`).

## The question

Two quirks appear while playing with TWASE:

1. clicks stop registering (first noticed in the diplomacy menu; tabbing out and back in temporarily restores them);
2. the rendered picture then shrinks and expands in a loop, while the macOS window stays full size.

The same two quirks appear with a community byte patch unrelated to TWASE (building-slot UI). So the trigger is "the
game's code was modified", not anything specific to TWASE.

## Verdict

**The game has an anti-tamper layer that hashes its entire `__text` section and reacts silently.** Any changed byte in
the executable's code, in memory or on disk, is detected. That covers TWASE's byte patches, its Dobby inline hooks, and
the community patch alike. Nothing crashes or logs; a mismatch only changes hidden state.

| Claim | Confidence | Evidence |
|---|---|---|
| Startup guards hash all of `__text` and notice a single changed byte | **High** | Emulated: range `0x100012020`–`0x104144bb4`; flipping one byte at `0x101234568` changes what each guard writes (below) |
| TWASE's patches are already applied when those guards run | **High** | `libTWASE.dylib` patches from its constructor (`src/platform/macos/dylib/main.cpp`); dyld runs inserted libraries' initializers before the main executable's |
| The same layer causes the click/shrink quirks | **Medium** | Identical symptoms across unrelated patches; guard clusters sit in the display/Metal-view and UI code. The code that turns the hidden state into visible behaviour is **not found yet** |

### Correction to the earlier version of this note

The earlier version said there was no anti-tamper. Two things went wrong:

- **Wrong binary.** Its addresses (`sub_10039c358`, `sub_10056cefc`, …) come from `~/GitHub/open-twa/bn/Total War
  ATTILA.bndb`, which is the **Mac App Store build** (1.6.1 RC4, `480969.104294`, UUID `196C74A0-…`,
  `/Applications/Total War ATTILA.app`). Bytes at the same VAs differ from the Steam build TWASE targets.
- **Wrong method.** It listed the callers of memory/signature imports. A process reads its own code with plain loads,
  so no import is needed. The guards here use none: they get a base address through a GOT slot, then run an inlined
  xxHash64 loop.

## The anti-tamper layer

### Building blocks

| Piece | Address (Steam) | What it is |
|---|---|---|
| `xxh64` | `0x1002bd140` | Plain xxHash64 (seed argument); `FeralBuffer_xxh64` `0x10025f584` / `FeralBuffer_xxh64_xor` `0x1003bbdfc` wrap it for Feral's buffer type |
| Anchor | `COpenGLProgramAspectManager::InitialiseThreadLocals` `0x10025cc64`, reached through GOT `0x104a7f590` (weak bind) | Base address for the range guards. Its own body loads the same GOT slot; its branch target `0x10025ccc8` holds the string "(c) 2014 - Cryptic Apps SARL - Disassembling not allowed." |
| Obfuscation noise | ~400 functions | Thread-local scratch arrays (TLV descriptors `0x105012220/38/50`) mutated around every guard, plus an obfuscated event scheduler (`EventCommand…`, handles XOR-masked with `0xe42a4822042a4822` etc.). The scheduler is also used for ordinary things (smooth scrolling in `-[CFeralNSWindow scrollWheel:]`, the screen-capture check in `mouseEntered:`) |

Guards **never branch** on a mismatch. They compute `h = hash ^ expected`, fold it to 32 bits (`(h >> 32) | h`, which is
zero only if the hashes match) and add it to the counter or to their own state. Whatever reads that state later is what
misbehaves.

### Code guards (cover the main executable's `__text`)

| Guard | Address | When | Range |
|---|---|---|---|
| `AntiTamper_StartupTextHash_55` | `0x100259b28` (`mod_init_func_55`) | Static initializer, once at load | All of `__text`, `0x100012020`–`0x104144bb4` (emulated) |
| `AntiTamper_StartupTextHash_100` | `0x10042e430` | same | same |
| `AntiTamper_StartupTextHash_199` | `0x100aa8d80` | same | same |
| `AntiTamper_StartupTextHash_227` | `0x100bc2f04` | same | same |
| `AntiTamper_StartupTextHash_236` | `0x100c157e8` | same | same |
| `mod_init_func_532` | `0x100fbeac0` | same | Same template; emulation stopped early at a call it didn't model |
| `AntiTamper_TextRangeGuard_*` | `0x100257c8c`, `0x10042b434`, `0x100aa56c0`, `0x100bbd54c`, `0x100c0c318`, `0x100f81860` | No direct callers: run later through the obfuscated scheduler | `[anchor − a, anchor + b]`, with `a`/`b` decoded at run time from obfuscated tables (e.g. `0x104f6cd66` for `0x100bbd54c`); inlined xxh64; result goes to the hidden counter |
| `AntiTamper_AnchorPageWriteProbe` | `0x10025cd10` | — | Makes the anchor page writable (`sub_1002cb818`, a `vm_region`/`mprotect` wrapper), writes `0xcc`, restores it; if that fails, adds 3 to the counter |

Emulating each startup guard with one flipped `__text` byte changes exactly three writes: the guard's own state word
and a byte in an obfuscated table:

| Guard | State word: clean → patched | Table byte: clean → patched |
|---|---|---|
| `_55` | `0x105019000`: `0x100226` → `0x500226` | `0x104da2ef0`: `f6` → `5d` |
| `_100` | `0x10504ef00`: `0x23c9c` → `0x223c9c` | `0x104f6d53d`: `43` → `b0` |
| `_199` | `0x105060d20`: `0xb21641` → `0x1b21641` | `0x104f762d2`: `33` → `89` |
| `_227` | `0x105065460`: `0x18dca` → `0x38dca` | `0x104d9f52d`: `d9` → `2a` |
| `_236` | `0x105065fb0`: `0x1d82dc` → `0x3d82dc` | `0x104f6be60`: `ad` → `e7` |

The expected hashes are therefore compile-time constants: a patch applied before the initializers is **not** baked into
a baseline. The table bytes sit in the tables the run-time range guards decode their parameters from (e.g.
`0x104f6be43…` is read by `0x100aa56c0`). So the damage chains: startup mismatch → corrupted table → later guards
decode wrong values → hidden counter grows.

The guards are spread over the binary's modules. Each module has one startup guard and one periodic guard:
`0x10025…` (render setup), `0x10042…`, `0x100aa…` (the Metal view: `-[CFeralMetalView …]` `0x100a9e628`, the
`DisplaySafeAreaPadding` reference `0x100aa2a20`), `0x100bb…`/`0x100bc…`, `0x100c0…`/`0x100c1…`, and
`0x100f8…`–`0x100fb…` (UI: the `HasNotch`, `notch_pos`, `NotchCompatibilityModeActive` references at `0x100f91088`,
`0x100f9f690`, `0x100f8ad4c`).

### Other checks (not affected by TWASE)

| Check | Address | What it verifies |
|---|---|---|
| `SteamApi_LoadVerifiedLibrary` | `0x1000406fc` | xxh64 of `Contents/Frameworks/libsteam_api.dylib` on disk must be `0x0b8f001cf3bc9596` (verified against the file) before `dlopen`. The `SteamAPI_*` names it looks up are XOR-obfuscated (`0x1000415b4`…`0x10004c128`) |
| `SteamApi_ScheduleDelayedHashRecheck` / `…_Callback` | `0x100040444` / `0x10004cae0` | Re-hashes the library on a detached thread (`Thread_StartDetached` `0x100092f14`) after a random 10.007–10.986 s; a mismatch is added to the hidden counter |
| `AntiTamper_LoadVerifiedModule` | `0x10033d924` | Hashes a blob, then loads it with a custom Mach-O loader (`MachO_LoadImageFromBuffer` `0x100504398`, starts by parsing a fat header). On failure it `dlclose`s the libraries it opened and `munmap`s the blob |
| `AntiTamper_PeriodicModuleHashCheck` | `0x10033dd00` | Scheduled every 419 s by `AntiTamper_CreateModuleWatcher` `0x10033db98`; re-hashes the regions of that manually loaded module (a `std::map` of ptr/len/expected) into the hidden counter |
| `CodeSign_CheckSelfAgainstRequirement` | `0x10039bb70` | `SecCodeCopySelf` + `SecCodeCheckValidity` against an embedded requirement. Wrappers: `CodeSign_CheckSelf_AnchorAppleGeneric` `0x10039b9dc` (`anchor apple generic`) and `CodeSign_CheckSelf_LeafCertPin` `0x10039bc68` (`certificate leaf = H"e07adea9cf6b76950903beea4687b1cd8759de76"`). No static references found. Runtime logs show `SecCodeCheckValidity` stays valid under TWASE, since it checks the signature state, not the live bytes |
| `CodeSign_StaticCheckRequirementString` | `0x10056ef3c` | `SecStaticCodeCreateWithPath` + `SecStaticCodeCheckValidity` with string requirements (entitlement gating, called from `0x10056e3dc`, `0x10056ec14`) |
| `MainImage_WalkLoadDylibCommands` | `0x100f13bd4` | Walks the main executable's `LC_LOAD_DYLIB` commands. Libraries added by `DYLD_INSERT_LIBRARIES` are not load commands of the executable, so TWASE is not listed there |
| `sub_100faf460` | — | Compares 16-byte entries (UUID-sized) from a vector at `0x1050190b8` against a 43-entry table at `0x1050715d0` and writes the counter. Purpose not confirmed |

## How the quirks fit

- **Any code edit triggers it.** TWASE patches from its constructor, before the game's static initializers. The
  community patch changes the file on disk. Both change `__text`, which the startup guards compare against constants.
- **Delayed and progressive.** Startup guards only corrupt state. The range guards and the module watcher run later
  (seconds to minutes) through the scheduler and keep adding to the counter. That matches "fine at first, then clicks
  fail, then the picture pulses".
- **Window untouched.** The `[Window]` log keeps the `NSWindow` (1512×945 fullscreen) and the Metal drawable
  (2304×1440) constant while the picture shrinks. The damage is in game-side values (viewport/scale or input mapping),
  which fits corrupted internal state rather than a real resize.
- **Tabbing out "fixes" clicks for a while.** `-[CFeralNSApplication applicationDidBecomeActive:]` /
  `-[CFeralNSWindow onWindowDidBecomeKey:]` re-send activation events, re-run `updatePresentationOptions` and
  re-initialise input state, until the damaged state takes over again. This is inferred: the code path from the counter
  to input is not found.

Context, not causes: Feral's display settings in `Preferences Data` (`EnableNotchCompatibilityMode = 1`,
`NotchCompatibilityModeActive = 1`, `ForceSystemFullscreen = 1`, `HideSystemUIInFullscreen = 1`) and the notch
handling (`NotchScale`, `renderUnderNotch`, `notch_pos_*`) are the most likely visible victims of the "picture shrinks"
symptom.

## Open points

1. **Who reads the hidden counter / guard state and what it changes.** Every site found so far *adds* to it. The reader
   probably goes through another masked alias or an obfuscated table. Next steps: emulate `AntiTamper_InitHiddenCounter`,
   get the heap slot, then watchpoint it in a live session (lldb) or search for loads through the
   `0x1055d6740/48` and `0x1055d6750/58` aliases that feed a comparison.
2. `mod_init_func_532` (`0x100fbeac0`) range: same template, needs a stub for the call at `0x1001287b0` to emulate it.

## Tests that would settle it

1. **No code edits, library still injected.** `[dev] disable = ["unit_size_patch", "lua_log", "game_tick", "win_main",
   "mod_loader", "diplomacy", "region_trading", "feral_mod_order"]` (currently set in the install's `config.ini`). The
   Metal overlay and input monitor only swizzle Objective-C methods and change no `__text` bytes. Prediction: **no
   quirks** in a long fullscreen session.
2. **One trivial byte patch.** Only `unit_size_patch` on. Prediction: the quirks come back. That would show a single
   changed instruction is enough.
3. **Late patching.** Apply the patches after the static initializers have run (e.g. from the first game tick instead
   of the dylib constructor). This avoids the startup guards but not the run-time range guards, so the quirks should
   come later or be weaker. That would separate the two guard stages.

# Integrity checks and the in-game quirks

Build: Feral 1.6.1 RC2 (Steam), `CFBundleVersion 480285.103778`, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326`.
Addresses are unslid VAs (image base `0x100000000`); runtime = VA + ASLR slide. Found with Binary Ninja over MCP,
Unicorn emulation and runs of the real game, see [re-tooling.md](re-tooling.md). The functions below are named in the
Steam database (`<game root>/Total War ATTILA/Total War ATTILA.bndb`).

## The question

Two quirks appear while playing with TWASE:

1. clicks stop registering (first noticed in the diplomacy menu; tabbing out and back in temporarily restores them);
2. the rendered picture then shrinks and expands in a loop, while the macOS window stays full size.

The same two quirks appear with a community byte patch unrelated to TWASE (building-slot UI). So the trigger is "the
game's code was modified", not anything specific to TWASE.

## Verdict (2026-10-27)

**The click quirk is driven by a hidden "seconds since the first input event" clock inside Feral's event dispatcher.**
Every session longer than about 8 minutes lost clicks around the 8-minute mark, and no shorter one did. Keeping that
clock reset (`[dev] reset_input_clock`) made a 13+ minute session with all TWASE features play normally.

Still open: does the clock only block input when the game has detected changed code? The startup code hash does record
TWASE's patches. The community patch reproducing the quirk without TWASE points that way. The deciding run is a long
session with no code changes and no reset.

| Claim | Status | Evidence |
|---|---|---|
| Clicks stop after ~8 min, in growing intervals | **Observed** | Sessions 8, E, F (10–14 min): yes, at ~8 min; sessions A, C, D (3.5–7 min): no. The game cursor's click animation stops too, so the game never takes the click |
| The clicks reach the game | **Confirmed** | Every left click arrives at the app within 1–10 ms while clicks fail; no main-thread stalls |
| The game keeps a hidden input clock | **Confirmed** | `FeralEventSource_Dispatch` `0x10056c1c4` (below); live values match system uptime |
| Resetting the clock prevents the quirk | **Confirmed once** | Session G: all features, clock reset every 5 s, 13+ min without the quirk |
| The clock only blocks input when code was changed | **Open** | Needs a >10 min session with no code changes and no reset (and one vanilla) |
| Startup guards hash all of `__text` against built-in constants | **Confirmed** | Emulation: range `0x100012020`–`0x104144bb4`; one flipped byte changes their output |
| The real game records TWASE's patch at startup | **Confirmed** | All five state words in a live `__DATA` dump equal the emulated "patched" values, not the clean ones (table below) |
| Run-time range guards react to changed code | **No** | Live-state emulation: their complete output is identical with and without a flipped byte |
| The hidden counter tracks code tampering | **No** | Stays 0 in every run |
| Something reads the startup state during play | **None seen** | Hardware watchpoints on four of the five state words: no access in menu or campaign |

### The input clock

Every input event (`-[CFeralNSWindow dispatchEvent:]`, `-[CFeralNSApplication dispatchEvent:]`, both tail-call it) runs
this inside `FeralEventSource_Dispatch` (`0x10056c1c4`), written as branch-free obfuscated code:

```
t      = (uint32) event.timestamp                         // NSEvent timestamp, seconds of system uptime
start  = *(int**)0x1055d8158                              // plain pointer
elapsed= *(int**)(((&0x1055d8198 ^ ~[0x1055d81a0]) - (int)[0x1055d817c]) << 3)   // masked pointer
*start = *start ? *start : t                              // first input event, set once
if (*elapsed > t) *start = t
*elapsed = t - *start                                     // seconds since the first input event
```

The event is then passed on (`data_1055d81d0`'s handler, then `sub_100394f9c(&data_1055d73e0, …)`). Other code that
reads `*elapsed`: `sub_10056c700` (vector-growing, many callers) and `sub_100cc2268` (called from `sub_100cc41e4`), both
in the same obfuscated style. Which of them turns the elapsed time into dropped clicks, and on what condition, is not
decoded yet.

`[dev] reset_input_clock` sets `*start` to the current uptime every 5 s, so `*elapsed` never exceeds ~5 s. The probe
logs `[InputClock] start … elapsed … (now …)` every 5 s either way.

### Correction to the first version of this note

The first version said there was no anti-tamper. Two things went wrong:

- **Wrong binary.** Its addresses (`sub_10039c358`, `sub_10056cefc`, …) came from `~/GitHub/open-twa/bn/Total War
  ATTILA.bndb`, which is the **Mac App Store build** (1.6.1 RC4, `480969.104294`, UUID `196C74A0-…`,
  `/Applications/Total War ATTILA.app`). Bytes at the same VAs differ from the Steam build TWASE targets.
- **Wrong method.** It listed the callers of memory/signature imports. A process reads its own code with plain loads,
  so no import is needed. The guards here use none: they get a base address through a GOT slot, then run an inlined
  xxHash64 loop.

The second version went too far the other way. It assumed the startup corruption chains through the range guards into a
hidden counter that something reads. The runtime checks below disproved the middle of that chain.

## The anti-tamper layer

### Building blocks

| Piece | Address (Steam) | What it is |
|---|---|---|
| `xxh64` | `0x1002bd140` | Plain xxHash64 (seed argument); `FeralBuffer_xxh64` `0x10025f584` / `FeralBuffer_xxh64_xor` `0x1003bbdfc` wrap it for Feral's buffer type |
| Anchor | `COpenGLProgramAspectManager::InitialiseThreadLocals` `0x10025cc64`, reached through GOT `0x104a7f590` (weak bind) | Base address for the range guards. Its own body loads the same GOT slot; its branch target `0x10025ccc8` holds the string "(c) 2014 - Cryptic Apps SARL - Disassembling not allowed." |
| Hidden counter | `AntiTamper_InitHiddenCounter` (`mod_init_func_56`, `0x10025a6ec`) | At load: a random-length heap array of random ints, one slot zeroed. The pointer to the slot is stored plainly at `0x105019040` and XOR/offset-masked in `0x1055d6720/30/38`, `0x1055d6724/40/48`, `0x1055d6728/50/58` (all three decode to `0x105019040`) |
| Obfuscation noise | ~400 functions | Thread-local scratch arrays (TLV descriptors `0x105012220/38/50`) mutated around every guard, plus an obfuscated event scheduler (`EventCommand…`, handles XOR-masked with `0xe42a4822042a4822` etc.). The scheduler is also used for ordinary things (smooth scrolling in `-[CFeralNSWindow scrollWheel:]`, the screen-capture check in `mouseEntered:`) |

Guards **never branch** on a mismatch. They compute `h = hash ^ expected` and fold it to 32 bits (`(h >> 32) | h`, which is
zero only if the hashes match). The result is mixed into obfuscated state with branch-free selects, so the only way to
see an effect is to compare complete outputs.

### Code guards (cover the main executable's `__text`)

| Guard | Address | When | Range | Effect of changed code |
|---|---|---|---|---|
| `AntiTamper_StartupTextHash_55` | `0x100259b28` (`mod_init_func_55`) | Static initializer, once at load | All of `__text`, `0x100012020`–`0x104144bb4` | Sets a bit in its state word (`0x105019000`), writes a different byte into its table (`0x104da2ef0`) |
| `AntiTamper_StartupTextHash_100` | `0x10042e430` | same | same | Same pattern: `0x10504ef00` / `0x104f6d53d` |
| `AntiTamper_StartupTextHash_199` | `0x100aa8d80` | same | same | `0x105060d20` / `0x104f762d2` |
| `AntiTamper_StartupTextHash_227` | `0x100bc2f04` | same | same | `0x105065460` / `0x104d9f52d` |
| `AntiTamper_StartupTextHash_236` | `0x100c157e8` | same | same | `0x105065fb0` / `0x104f6be60` |
| `mod_init_func_532` | `0x100fbeac0` | same | Same template; not emulated (stops at a call it didn't model) | — |
| `AntiTamper_TextRangeGuard_*` | `0x100257c8c`, `0x10042b434`, `0x100aa56c0`, `0x100bbd54c`, `0x100c0c318`, `0x100f81860` | No direct callers; the obfuscated scheduler (or `[dev] run_tamper_guards`) | All of `__text` (emulated with live state for `0x100bbd54c`) | **None.** Each writes its PRNG state (2 words) and re-encodes its table byte (same byte as the startup guard of its module) on every call; the values don't depend on the code bytes |
| `AntiTamper_AnchorPageWriteProbe` | `0x10025cd10` | — | — | Makes the anchor page writable (`sub_1002cb818`), writes `0xcc`, restores it; if that fails, adds 3 to the hidden counter |

Startup state in the live game (2026-10-27, slide `0x46c4000`, `unit_size_patch` applied by TWASE before the game's
initializers), compared with emulating the same guard at the same slide:

| Guard | Emulated, clean code | Emulated, `unit_size_patch` | Live game |
|---|---|---|---|
| `_55` (`0x105019000`) | `0x8a38` | `0x48a38` | **`0x48a38`** |
| `_100` (`0x10504ef00`) | `0x312aa29` | `0x712aa29` | **`0x712aa29`** |
| `_199` (`0x105060d20`) | `0x2c9a64` | `0x6c9a64` | **`0x6c9a64`** |
| `_227` (`0x105065460`) | `0xb67703` | `0x1b67703` | **`0x1b67703`** |
| `_236` (`0x105065fb0`) | `0x4bb567fd` | `0xcbb567fd` | **`0xcbb567fd`** |

The state words depend on the ASLR slide (the PRNG is seeded from an address), so compare them only within one run.
The expected hashes are built in, so a patch applied before the initializers is detected, not baked into a baseline.

The guards are spread over the binary's modules, one startup guard and one range guard each: `0x10025…` (render setup),
`0x10042…`, `0x100aa…` (Metal view: `-[CFeralMetalView …]` `0x100a9e628`, `DisplaySafeAreaPadding` reference
`0x100aa2a20`), `0x100bb…`/`0x100bc…`, `0x100c0…`/`0x100c1…`, `0x100f8…`–`0x100fb…` (UI: `HasNotch`, `notch_pos`,
`NotchCompatibilityModeActive` references at `0x100f91088`, `0x100f9f690`, `0x100f8ad4c`).

### Other checks (not affected by TWASE)

| Check | Address | What it verifies |
|---|---|---|
| `SteamApi_LoadVerifiedLibrary` | `0x1000406fc` | xxh64 of `Contents/Frameworks/libsteam_api.dylib` on disk must be `0x0b8f001cf3bc9596` (verified against the file) before `dlopen`. The `SteamAPI_*` names it looks up are XOR-obfuscated (`0x1000415b4`…`0x10004c128`) |
| `SteamApi_ScheduleDelayedHashRecheck` / `…_Callback` | `0x100040444` / `0x10004cae0` | Re-hashes the library on a detached thread (`Thread_StartDetached` `0x100092f14`) after a random 10.007–10.986 s; a mismatch is added to the hidden counter |
| `AntiTamper_LoadVerifiedModule` | `0x10033d924` | Hashes a blob, then loads it with a custom Mach-O loader (`MachO_LoadImageFromBuffer` `0x100504398`, starts by parsing a fat header). On failure it `dlclose`s the libraries it opened and `munmap`s the blob |
| `AntiTamper_PeriodicModuleHashCheck` | `0x10033dd00` | Scheduled every 419 s by `AntiTamper_CreateModuleWatcher` `0x10033db98`; re-hashes the regions of that manually loaded module (a `std::map` of ptr/len/expected) into the hidden counter |
| `CodeSign_CheckSelfAgainstRequirement` | `0x10039bb70` | `SecCodeCopySelf` + `SecCodeCheckValidity` against an embedded requirement. Wrappers: `CodeSign_CheckSelf_AnchorAppleGeneric` `0x10039b9dc` (`anchor apple generic`) and `CodeSign_CheckSelf_LeafCertPin` `0x10039bc68` (`certificate leaf = H"e07adea9cf6b76950903beea4687b1cd8759de76"`). No static references found. `SecCodeCheckValidity` stays valid under TWASE: it checks the signature state, not the live bytes |
| `CodeSign_StaticCheckRequirementString` | `0x10056ef3c` | `SecStaticCodeCreateWithPath` + `SecStaticCodeCheckValidity` with string requirements (entitlement gating, called from `0x10056e3dc`, `0x10056ec14`) |
| `MainImage_WalkLoadDylibCommands` | `0x100f13bd4` | Walks the main executable's `LC_LOAD_DYLIB` commands. Libraries added by `DYLD_INSERT_LIBRARIES` are not load commands of the executable, so TWASE is not listed there |
| `sub_100faf460` | — | Compares 16-byte entries (UUID-sized) from a vector at `0x1050190b8` against a 43-entry table at `0x1050715d0` and writes the hidden counter. Purpose not confirmed |

## Runtime runs (2026-10-26/27, windowed, Steam build)

| Run | Config | Result |
|---|---|---|
| 1 | No code changes (all patches/hooks disabled) | Hidden counter 0 for 8.5 min |
| 2 | Only `unit_size_patch` | Counter 0 for 8 min |
| 3 | Patch + `run_tamper_guards` | All six range guards run (4–11 ms each, about one xxh64 pass over 68 MB); counter stays 0 |
| 4 | Same, plus one `__text` byte flipped at runtime (`0x10025ccc8`) | Counter stays 0 |
| 5/6 | Same, with `__DATA` diffs and dumps around each guard call | Each guard writes only its 2 PRNG words and its table byte; the dump emulated offline reproduces these writes exactly |
| 7 | Patch + `watch_tamper_state`, main menu | Self-test hit caught; no game access to the state words |
| 8 | Same, campaign loaded and played | Windowed until 00:37, then switched to **fullscreen**. After repeated diplomacy actions **the quirks appeared**. Over the whole session: no game access to the state words, counter 0 |
| A | No code changes, fullscreen | ~7 min, no quirks (shorter than the ~8 min onset) |
| C | Only `unit_size_patch`, fullscreen; window `frameState` logged | ~5 min, no quirks; `frameState` stayed 0 (no title-bar drag) |
| D | Same, plus a hardware breakpoint on the patched branch | ~3.5 min, no quirks; the patched loop's list never exceeded 29 entries (behaviour only changes at 64) |
| E | All features but `lua_log` | ~10 min, **quirks at ~8 min**; `frameState` 0 throughout |
| F | Same, plus click-delivery and main-thread stall logging | ~10 min, **quirks at ~8 min**; every click delivered within 1–10 ms, no stalls |
| G | Same, plus `reset_input_clock` | **13+ min without the quirk**; `elapsed` stays at 1–5 s |

The `[dev]` options used are documented in [re-tooling.md](re-tooling.md#runtime-probes-in-twase).

## Open points

1. **Does the clock block input without changed code?** Run TWASE with all patches/hooks disabled and no reset for
   more than 10 minutes, and run the vanilla game from Steam the same way. If both stay clean, the gate depends on the
   startup tamper state; if not, it's time-based regardless of mods.
2. **Which reader of `*elapsed` drops the clicks, and its condition**: `sub_10056c700`, `sub_100cc2268`. Live-state
   emulation (dump `__DATA`, run them at the runtime slide) or a hardware watchpoint on the `*elapsed` heap slot would
   show it.
3. Readers of the startup **table bytes** (`0x104da2ef0`, `0x104f6d53d`, `0x104f762d2`, `0x104d9f52d`,
   `0x104f6be60`) and of the fifth state word (`0x10504ef00`, not watched: only four watchpoint registers).
4. Whether the startup state changes anything that isn't a memory access by game code, e.g. what the obfuscated
   scheduler queues. Next step: snapshot the scheduler queue at `~data_104d9c5a0` in both configurations.
5. `mod_init_func_532` (`0x100fbeac0`) range and effect.

## Context for the quirks (not causes)

- The `[Window]` log keeps the `NSWindow` and the Metal drawable (2304×1440) constant while the picture shrinks, so the
  change happens in game-side values (viewport, scale or input mapping).
- Tabbing out runs `-[CFeralNSApplication applicationDidBecomeActive:]` / `-[CFeralNSWindow onWindowDidBecomeKey:]`,
  which re-send activation events and re-run `updatePresentationOptions`.
- Feral's display settings in `Preferences Data` (`EnableNotchCompatibilityMode = 1`, `NotchCompatibilityModeActive =
  1`, `ForceSystemFullscreen = 1`, `HideSystemUIInFullscreen = 1`) and the notch handling (`NotchScale`,
  `renderUnderNotch`, `notch_pos_*`) are candidates for the "picture shrinks" symptom regardless of tampering.

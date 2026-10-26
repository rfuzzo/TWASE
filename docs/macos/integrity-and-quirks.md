# Integrity checks and the in-game quirks

Build: Feral 1.6.1 RC2, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326`. Addresses are unslid VAs (image base
`0x100000000`); runtime = VA + ASLR slide. Found with Binary Ninja (MCP) against the Mac game binary.

## The question

Two quirks appear while playing with TWASE:

1. clicks stop registering (first noticed in the diplomacy menu; tabbing out and back in temporarily restores them);
2. the rendered picture then shrinks and expands in a loop, while the macOS window stays full size.

The working hypothesis was Feral's anti-tamper: the game notices TWASE's byte patches / hooks and degrades itself.
The user then found the **same two quirks** with a community byte patch unrelated to TWASE (building-slot UI). So the
trigger is "the game's code was modified", not anything specific to TWASE's region trading. This note pins down
whether a Feral integrity mechanism is behind that, and whether the two quirks are one thing.

## Verdict

**No evidence of an anti-tamper mechanism that detects the patches and degrades the game.** The integrity machinery
that exists checks the **on-disk signature / entitlements**; it cannot see in-memory byte patches or inline hooks, and
the one self-check is not even called. This matches the earlier runtime measurements: `csops` flags and
`SecCodeCheckValidity` stayed valid the whole session, with every TWASE patch and hook applied.

So the quirks are almost certainly a genuine engine/UI fault provoked by modifying code, not Feral punishing the mod.
Confidence: **high** that it is not the signature/entitlement checks below; **medium** on the mechanism of the quirks
themselves (argued from behaviour, see the last section).

## Every self-inspection primitive in the binary

Found by walking the callers of each memory/signature import (`SecCode*`, `SecStaticCode*`, `mach_vm_*`,
`vm_region*`, `getsectbyname`, `_dyld_*`). That is the complete set — nothing else can read the process's own code.

### Signature checks (the real integrity code)

| What | Address | Called? | What it actually checks |
|---|---|---|---|
| Runtime self-check: `SecRequirementCreateWithData` → `SecCodeCopySelf` → `SecCodeCheckValidity` | `sub_10039c358` | **No** | The running process against an embedded designated requirement (its own cdhash). |
| └ wrappers that build the requirement blob and call it | `sub_10039c1c4`, `sub_10039c450` | **No callers** (0 code, 0 data xrefs) | Dead in the running game. The blobs are `0xfade0c00` SecRequirement structs (one is a cdhash requirement). |
| On-disk check: `SecRequirementCreateWithString` → `SecStaticCodeCheckValidity` | `sub_10056da00` | **Yes** | A signed file against a requirement string. |
| └ builds `entitlement["…"]` requirements (XOR-obfuscated strings) | `sub_10056d708` | Yes | Whether the code has a given **entitlement**. |
| └ runs a battery of them, stores each result as a named `NVariantValue<bool>` | `sub_10056cefc` | Yes (startup) | Feature / store / DLC gating, computed once into a settings dictionary. |

Key points:

- `SecCodeCheckValidity` / `SecStaticCodeCheckValidity` validate the **signed code on disk** (the sealed cdhash). They
  do not re-hash the live `__TEXT` pages, so TWASE's inline hooks and in-memory byte patches are invisible to them.
  The on-disk file is untouched, so these pass.
- The **self**-check (`sub_10039c358` and its wrappers) has no references at all — it is not invoked while the game
  runs. The only live use of this family is the **entitlement gating** in `sub_10056cefc`, which is about which
  features/DLC are unlocked, not tamper detection, and runs once at startup.
- The binary is signed `Developer ID Application: Feral Interactive Ltd (XH73Y9BGP8)` with hardened runtime
  (`flags=0x10000`). Nothing re-verifies it in memory.

### Memory introspection that is *not* anti-tamper

| What | Address | Purpose |
|---|---|---|
| `mach_vm_region` + `task_info(TASK_VM_INFO)` + `getrusage`, sums region sizes | `sub_100bc4588` | Memory-footprint telemetry. |
| `mach_vm_read_overwrite` thin wrappers (6) + Mach-O header parse (`0xfeedfacf` magic) | `sub_10078a1b4…4dc`, `sub_100793f98` | Crash reporter / symbolication (reads images in the task). |
| `getsectbyname("__TEXT"/"__DATA,__const")` + `_dyld_get_image_vmaddr_slide` | `sub_1001285e0` | Classifies whether a pointer lies in the module's `__text`/`__const` — a **string-literal test**, not a checksum. |
| `vm_region_64` → returns a page's protection bits (`info & 7`); caller rounds to page size | `sub_10012856c`, `sub_1002cb568` | Page-accessibility probe, used by `COpenGLProgramAspectManager::InitialiseThreadLocals` (graphics layer). |

None of these reads `__text` and compares it against a stored hash. There is **no periodic code-checksum thread**.

### Absent (things a tamper/DRM layer would usually have)

- No `ptrace` import, so no `PT_DENY_ATTACH` anti-debug.
- No anti-tamper / "modified" / "integrity" / "cracked" diagnostic strings (only unrelated matches like
  `ca_signature`, graphics `*_SIGNATURE`, serialization "was modified").
- No code that hashes its own code pages and branches on a mismatch.

## Are the two quirks connected?

Not provable from the binary alone, but the behavioural evidence points to **one escalating fault, not two**:

- They always occur **in the same order** — clicks stop first, the shrink/expand loop second — and across *unrelated*
  code modifications (TWASE hooks, TWASE byte patches, the community building-slot patch). Independent edits producing
  the same specific, ordered symptoms indicates one shared downstream state going wrong.
- The macOS `NSWindow` and the Metal drawable never change size during the shrinking (logged: 1512×945 fullscreen,
  drawable 2304×1440). So the loop is **inside the game's own UI scaling / layout**, not the window.
- "Tabbing out and back in restores clicks" fits a **layout / hit-test desync**: a focus change forces the game to
  relayout, momentarily realigning where it thinks the UI is with where it is drawn. Stage 1 = hit-testing offset
  (clicks miss their targets), stage 2 = the same scale/layout feedback getting worse until it oscillates visibly.

So the likely single cause is a corrupted UI scale/layout state, with the clicks and the shrink/expand being two
stages of it — a rendering/UI bug, consistent with finding no anti-tamper.

**Open gap:** why editing *unrelated* code regions provokes a UI-layout fault at all. The common factor may be the
injection method or timing rather than the specific bytes. This needs an in-game test, not more static RE.

## Tests that would settle it

1. **Code untouched, library still injected.** Disable every patch and hook, leaving only the overlay (which adds no
   game-code change):
   `[dev] disable = ["unit_size_patch", "lua_log", "game_tick", "win_main", "mod_loader", "diplomacy", "region_trading", "feral_mod_order"]`
   Long fullscreen session. Quirks still appear ⇒ it is the injected dylib / its overlay, not the code edits. No
   quirks ⇒ it is the act of modifying game code (any region).
2. **One trivial byte patch, nothing else.** Only `unit_size_patch` on. If that alone reproduces, the quirks need
   nothing but a single changed instruction, which rules out any TWASE-logic cause and matches the community-patch
   data point.
3. If 1 and 2 both reproduce, the fix belongs in whatever UI-scale/layout path desyncs, not in TWASE's features; the
   graphics-layer probe `COpenGLProgramAspectManager` and the game's render-scale handling are where to look next.

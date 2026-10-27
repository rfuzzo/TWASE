# Reverse-engineering toolkit (Mac) — quick start for agents

What is on this machine and how to drive it without a GUI. Everything here was used for
[integrity-and-quirks.md](integrity-and-quirks.md).

## Paths

| What | Path |
|---|---|
| Game (Steam, arm64, LC_UUID `392D6F66-…`) | `~/Library/Application Support/Steam/steamapps/common/Total War Attila/Total War ATTILA.app/Contents/MacOS/Total War ATTILA` |
| Mac App Store copy (different build, UUID `196C74A0-…`, **not** the one TWASE targets) | `/Applications/Total War ATTILA.app` |
| TWASE install, `config.ini`, logs (`twase-*.log`, `twase-launch.log`) | `<game root>/TWASE/`, `<game root>/TWASE/logs/` |
| Feral preferences (XML; display/notch/mouse settings live here) | `~/Library/Application Support/Feral Interactive/Total War ATTILA/Preferences Data` |
| Binary Ninja databases | Steam build (use this one; anti-tamper functions are named `AntiTamper_*`, `CodeSign_*`, `SteamApi_*`, `xxh64`): `<game root>/Total War ATTILA/Total War ATTILA.bndb`. **Mac App Store build** (1.6.1 RC4, other addresses): `~/GitHub/open-twa/bn/Total War ATTILA.bndb` |

Image base `0x100000000`; for `__TEXT`, file offset = VA − `0x100000000`. `__text` = `0x100012020`, size
`0x4132b94` (from `otool -l`).

## Binary Ninja over MCP (main tool)

The Binary Ninja GUI (if running, with `ui.mcp.enabled: true` in
`~/Library/Application Support/Binary Ninja/settings.json`) serves MCP over streamable HTTP at
`http://127.0.0.1:24642/mcp`. There is no Python API in this edition (no headless `import binaryninja`), so talk to the
MCP endpoint directly. Check it is up: `lsof -nP -iTCP -sTCP:LISTEN | grep binaryn`.

Minimal client (Python eval kernel; keep it in the kernel and reuse):

```python
import json, urllib.request
URL = "http://127.0.0.1:24642/mcp"; SID = None; _id = [0]

def rpc(method, params=None, notify=False):
    global SID
    _id[0] += 1
    body = {"jsonrpc": "2.0", "method": method}
    if params is not None: body["params"] = params
    if not notify: body["id"] = _id[0]
    h = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if SID: h |= {"MCP-Session-Id": SID, "MCP-Protocol-Version": "2025-11-25"}
    r = urllib.request.urlopen(urllib.request.Request(URL, json.dumps(body).encode(), h), timeout=600)
    SID = r.headers.get("MCP-Session-Id") or SID
    t = r.read().decode()
    if notify or not t: return None
    if t.startswith(("event:", "data:")):
        t = [l[5:] for l in t.splitlines() if l.startswith("data:")][-1]
    j = json.loads(t)
    if "error" in j: raise Exception(j["error"])
    return j["result"]

def call(name, **args):
    r = rpc("tools/call", {"name": name, "arguments": args})
    return "\n".join(c["text"] for c in r.get("content", []) if c.get("type") == "text")

rpc("initialize", {"protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "agent", "version": "0"}})
rpc("notifications/initialized", {}, notify=True)
```

Then:

1. `call("bn_binary_view_list")`: two databases are usually open, and they are **different builds** (the open-twa one
   is the Mac App Store RC4 build; same VA range, different bytes). Pick the Steam one with
   `call("bn_binary_view_set_active", binaryView="view_N")` and check the path with `bn_open_item_list`. Compare a few
   bytes with `bn_memory_read` against the game file if in doubt. Save renames with
   `bn_open_item_save(openItem=…)`.
   Note: `call(name, **args)` can't pass an argument named `name` (e.g. `bn_symbol_define`); call
   `rpc("tools/call", {"name": tool, "arguments": {...}})` directly.
2. Useful tools (75 in total, `rpc("tools/list", {})`):
   - `bn_function_search(query=…)` — ObjC methods are named (`-[CFeralNSWindow sendEvent:]`, …); C++ is mostly `sub_…`.
   - `bn_function_decompile(function="0x…")`, `bn_function_disassembly`, `bn_function_callers`, `bn_function_callees`.
   - `bn_string_list(query=…)` — finds strings by substring.
   - `bn_data_at(address=…)` — bytes, data variable and the **containing function** of an address.
   - Output is Markdown with a YAML header; strip it with `s.split("---", 2)[-1]`.
3. Limitation: there is **no "code references to a data address" tool** (`bn_data_xrefs_to` only returns data→data
   references, so it shows nothing for strings used by code). Use the ADRP scanner below.

## ADRP/ADD/LDR xref scanner (code → data references)

Decodes every `adrp` in `__text` plus the `add`/`ldr`/`str` that uses its register within the next 3 instructions.
17 M instructions, ~10 s with numpy. A few hits are false positives where the window crosses a `ret`; confirm with
`bn_function_disassembly`.

```python
import numpy as np, os
GAME = os.path.expanduser("~/Library/Application Support/Steam/steamapps/common/Total War Attila/"
                          "Total War ATTILA.app/Contents/MacOS/Total War ATTILA")
raw = open(GAME, "rb").read()
TEXT_VA, TEXT_SZ = 0x100012020, 0x4132b94
ins = np.frombuffer(raw[TEXT_VA - 0x100000000:][:TEXT_SZ], dtype="<u4").astype(np.int64)
pcs = TEXT_VA + 4 * np.arange(len(ins), dtype=np.int64)
is_adrp = (ins & 0x9F000000) == 0x90000000
rd = ins & 31
imm = (((ins >> 5) & 0x7FFFF) << 2) | ((ins >> 29) & 3)
imm = np.where(imm & (1 << 20), imm - (1 << 21), imm)
page = (pcs & ~0xFFF) + (imm << 12)
S, T = [], []
for k in (1, 2, 3):
    n = np.roll(ins, -k); rn = (n >> 5) & 31
    add = (n & 0xFF800000) == 0x91000000
    aimm = ((n >> 10) & 0xFFF) << (12 * ((n >> 22) & 1))
    ldst = (n & 0x3B000000) == 0x39000000
    scale = np.where((((n >> 26) & 1) == 1) & (((n >> 22) & 3) >= 2), 4, (n >> 30) & 3)
    limm = ((n >> 10) & 0xFFF) << scale
    for m, off in ((is_adrp & add & (rn == rd), aimm), (is_adrp & ldst & (rn == rd), limm)):
        i = np.nonzero(m)[0]; S.append(pcs[i]); T.append(page[i] + off[i])
src, tgt = np.concatenate(S), np.concatenate(T)
o = np.argsort(tgt, kind="stable"); src, tgt = src[o], tgt[o]

def xr(addr, span=1):  # code sites referencing [addr, addr+span)
    lo, hi = np.searchsorted(tgt, addr), np.searchsorted(tgt, addr + span)
    return sorted(set(int(x) for x in src[lo:hi]))
```

Map a site to its function with `json.loads(call("bn_data_at", address=hex(site)))["functions"]`.

Pointers in `__DATA*` use chained fixups (pointer format 6, `DYLD_CHAINED_PTR_64_OFFSET`; imports format 3,
`DYLD_CHAINED_IMPORT_ADDEND64`, 16-byte entries). In a rebase the low 36 bits are the target's offset from the image
base, so a raw search for `addr - 0x100000000` as 4 little-endian bytes finds most data pointers. Bit 63 marks a bind
whose low 24 bits are the import ordinal (e.g. GOT `0x104a7f590` = import 5808, the weak
`__ZN27COpenGLProgramAspectManager22InitialiseThreadLocalsEv`). For emulation, walk the chains properly: header at
`LC_DYLD_CHAINED_FIXUPS` `dataoff`, `starts_in_image` → per segment `starts_in_segment` → page starts, `next` = bits
51–62 in 4-byte strides.

## Emulating code with Unicorn

`python3 -m pip install --user unicorn capstone` works; in the eval kernel add `site.getusersitepackages()` to
`sys.path`. This was used to get the ranges the anti-tamper guards hash, which the decompiler can't show:

- Map `__TEXT`, `__DATA_CONST` and `__DATA` (`0x100000000`, size `0x56d8000`) from their file offsets, with the
  chained fixups applied (rebases → absolute VAs, binds → 0 or a chosen value; set GOT `0x104a7f590` to
  `0x10025cc64`).
- Thread-local variables: point every `__thread_vars` descriptor (`0x1050120b8`, `0x18` bytes each) at a stub that
  returns a scratch buffer per descriptor (the code does `ldr x8, [x0]; blr x8`).
- Calls into `__stubs` (`0x104144bb4`–`0x104155f20`): hook them, return a heap pointer in `x0`, jump to `lr`.
- `UC_HOOK_MEM_READ` on `0x100000000`–`0x104144bb3` catches the first code read; `UC_HOOK_MEM_WRITE` on `__DATA` and
  the fake heap shows what a function changes. Running it again with one flipped `__text` byte and diffing the writes
  shows whether a value depends on the code bytes. A full 68 MB xxh64 pass takes about a second.

The harness is about 60 lines of Python and isn't checked in; rebuild it from these notes.

### Emulating with the live game's state

Static data is often not enough: the guards decode their parameters from state set up by other initializers. Dump the
game's `__DATA`…`__common` from inside the process (`[dev] run_tamper_guards` writes `data-before.bin`,
`data-clean.bin` and `data-flipped.bin` to `TWASE/logs`, with the slide in the log), then emulate at the **runtime**
base (`0x100000000 + slide`). The masked pointers XOR in their own runtime addresses, so emulating at the unslid
base decodes them wrongly. Add the slide to every rebased slot in `__DATA_CONST`, set the GOT anchor to
`0x10025cc64 + slide`, map unmapped heap pages as zero on demand, and log them. On 2026-10-27 this reproduced the
writes of guard `0x100bbd54c` in the real game byte for byte.

## Running the game from an agent

Steam must be running. The game window opens on the user's screen, so tell them first.

- `"<game root>/twase-launch.command" --skip-launcher` starts the game with TWASE into the main menu;
  `~/GitHub/attila-mods/tools/play.sh --no-deploy --latest` loads the newest campaign save (with the repo's mod list).
  Run it as a background job and read `TWASE/logs/twase-*.log` (newest file).
- Quit with `osascript -e 'tell application id "com.feralinteractive.attilatw" to quit'`. It prints "User cancelled
  (-128)" but the game exits, and the launcher puts Feral's preferences back.
- `xmake` rebuilds and copies `libTWASE.dylib` into the game in about 2 s. Change `[dev]` options in
  `TWASE/config.ini`, not in the repo.

### Runtime probes in TWASE

`[dev]` options in `TWASE/config.ini` (Mac, reverse engineering only):

| Option | Does |
|---|---|
| `disable = [...]` | Skips patches/hooks by the name in the log, e.g. everything but `unit_size_patch` for a one-instruction test |
| `run_tamper_guards = true` | 30 s after start, calls the six `AntiTamper_TextRangeGuard_*` with the code as loaded, then with a never-executed `__text` byte flipped (and restored). Logs the hidden counter and every changed `__DATA` word per call, and dumps `__DATA` |
| `watch_tamper_state = true` | Hardware watchpoints (`ARM_DEBUG_STATE64`, 4 registers) on the startup guards' state words, plus an instruction breakpoint on the `unit_size_patch` branch (logs the walked list's length), armed on every thread every 5 s. Hits go to a thread-level Mach exception port and are logged as `[TamperWatch] … at pc … (lr …, x20 …)`; a self-test read at startup proves it works. Edit `Watched[]` / `Breakpoints[]` in `src/platform/macos/dylib/TamperWatch.cpp` |
| `reset_input_clock = true` | Sets Feral's hidden input clock start to the current uptime every 5 s, so the "seconds since the first input" value stays near 0; prevents the click quirk |
| `input_clock_offset = N` | Moves Feral's input clock start N seconds back once (when the clock first exists), so the game behaves as if N seconds of input time had passed. With a patch, `600` reproduces the click quirk within a minute |

Always on:

- `[Integrity] Feral tamper counter` (the heap int behind `0x105019040`), logged when it changes.
- `[InputClock] start … elapsed … (now …)` every 5 s: Feral's hidden input clock (see integrity-and-quirks.md).
- `[Input] left down/up delivered after N ms`: how long after the click AppKit hands it to the app.
- `[Stall] main thread blocked for N ms` (a watchdog thread pings the main queue every 100 ms) and `[Stall] TWASE's
  periodic check took N ms`.
- `[Window] … frameState … grabInput … scheduledDrag …`: `CFeralNSWindow` ivars (`frameState 2` = Feral's title-bar
  live drag).

## Shell tools

| Tool | Use |
|---|---|
| `otool -l <bin>` | Segments/sections (addresses, sizes, file offsets). |
| `strings -a <bin> \| grep -iE …` | Quick string survey before going to Binary Ninja. |
| `dwarfdump --uuid <bin>` | Confirm the build (must match `LC_UUID` in `docs/macos/addresses.md`). |
| `codesign -dv --entitlements - <bin>`, `codesign --verify --strict <app>` | Signature, hardened runtime flags, entitlements (the game has `allow-dyld-environment-variables`, `allow-unsigned-executable-memory`, `disable-library-validation`). |
| `plutil -p Info.plist` | Bundle settings (e.g. `NSPrefersDisplaySafeAreaCompatibilityMode`). |
| `grep -iE … "Preferences Data"` | Feral runtime settings (fullscreen, notch compatibility, mouse). |
| `lldb` | Available for live attach (game is hardened, attach needs SIP/entitlement allowances; not used so far). |

IDA Home 9.4 is installed (`/Applications/IDA Home (PC) 9.4.app`) but has no scripting bridge set up; the `xd://ida`
device can drive it if needed.

## Runtime evidence

- TWASE logs every 5 s when it changes: `[Window] …` (NSWindow frames, fullscreen/key state, CAMetalLayer drawable and
  bounds) from `src/platform/macos/dylib/Hooks/WindowState.mm`, and `[Integrity] …` (csops flags,
  `SecCodeCheckValidity`) from `src/platform/macos/dylib/Integrity.cpp`.
- `[dev] disable = [...]` in `TWASE/config.ini` skips individual patches/hooks (names in the log's `Skipping …` lines).

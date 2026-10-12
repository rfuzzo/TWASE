# apigen

Generates the campaign API reference in [docs/reference](../../docs/reference/README.md) (Markdown and `attila-api.json`).

## Inputs

| File | What | How it's made |
|---|---|---|
| `data/api_dump_macos.tsv` | what the game's Lua offers in a campaign: the registry's script classes and their methods, the scripts' globals and library tables (C or Lua), the events | `dump.lua` in the game (below) |
| `data/lunar_types_macos.json` | the script interfaces' method tables in the Mac binary: per table its methods with handler, arguments and return type | typed from the decompiled handlers in Binary Ninja (2026-10-06): return types from the push helpers, arguments from the pop helpers and `lua_type` checks, object classes from the RTTI of the object wrappers |
| the extracted `data.pack` | the game's Lua library (`script/_lib`, `lua_scripts`, `campaigns/<campaign>/*.lua`) for parameters and locations, and `lua_scripts/events.lua` | e.g. `twpack extract data.pack -o <folder>`; not in the repo (it's the game's content) |

The script classes are matched to the binary's method tables by their method names (each class has its table's methods plus `new` and `is_null_interface`, which every class gets from the binding template; the `GAME` object has a few more from another table).

## Running

```bash
python3 tools/apigen/apigen.py <extracted data.pack folder>
```

## Dumping the game's Lua

`dump.lua` writes `TWASE/api_dump.tsv` in the game folder. Run it in a campaign, as a script mod: copy it to `data/campaigns/main_attila/mods/zz_apidump/scripting.lua`, load a save, wait for the file (a few seconds after the campaign starts loading), quit the game and remove the folder again. Copy the file to `data/api_dump_macos.tsv`.

The dump is usually taken with mods enabled (the saves need them). `apigen.py` keeps only what the vanilla scripts define and what the engine provides, so mod globals don't end up in the reference.

Findings from the dump (2026-10-13):

- The campaign's `scripting.lua`, the library it requires and the script mods run in an environment of their own, not `_G` (the game's `require` loads files into the caller's environment); `_G` only has Lua's standard library and a few engine functions (`CliExecute`, `RequireRegister`, `system`, `vfs`).
- Lunar methods have no upvalues, so a method can't be mapped to its table entry from Lua.

## Limits

- Arguments: only what the known helpers read. Object arguments (a faction, a character, ...) are read by per-class helpers that aren't identified yet, and some numbers too, so an empty argument cell doesn't mean "none".
- Returns: lists and some objects go through helpers not identified yet.
- `UIComponent`, the `*_LIST_SCRIPT_INTERFACE` classes and a few UI classes have no method table in the scanned range of the binary yet: names only.
- Battle and front end: not dumped yet (only the campaign environment).

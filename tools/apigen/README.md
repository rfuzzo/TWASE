# apigen

Generates the campaign API reference in [docs/reference](../../docs/reference/README.md) (Markdown and `attila-api.json`).

## Inputs

| File | What | How it's made |
|---|---|---|
| `data/api_dump_macos.tsv` | what the game's Lua offers in a campaign: the registry's script classes and their methods, the scripts' globals and library tables (C or Lua), the events | `dump.lua` in the game (below) |
| `data/lunar_types_macos.json` | the script interfaces' method tables in the Mac binary: per table its methods with handler (and symbol), arguments and return type | `find_tables.py <game executable> --write` finds the tables in the Mach-O (no Binary Ninja needed), `bn_types.py` types the handlers from their decompiled code (Binary Ninja with the bndb open and its MCP server running) |
| the extracted `data.pack` | the game's Lua library (`script/_lib`, `lua_scripts`, `campaigns/<campaign>/*.lua`) for parameters and locations, and `lua_scripts/events.lua` | e.g. `twpack extract data.pack -o <folder>`; not in the repo (it's the game's content) |

The script classes are matched to the binary's method tables by their method names (each class has its table's methods plus `new` and `is_null_interface`, which every class gets from the binding template; the `GAME` object has a few more from another table).

## Running

```bash
python3 tools/apigen/apigen.py <extracted data.pack folder>
```

After a game update, or to improve the types:

```bash
python3 tools/apigen/find_tables.py "<game>/Total War ATTILA.app/Contents/MacOS/Total War ATTILA" --write
python3 tools/apigen/bn_types.py
```

## How the methods are typed

Campaign handlers get a context from `LuaGetLookup(L)` and read their arguments through helpers (Mac addresses, unslid):

| Helper | Address | Reads / pushes |
|---|---|---|
| `LuaPopString`, `LuaPopBool`, `LuaPopNumber` | `0x1038affd8`, `0x1038b23d0`, `0x1038b1ae0` | pop the top value; the Lua argument order is the reverse of the pops |
| `LuaPopInt` (two copies) | `0x1038b0a68`, `0x1038b2318` | pops a number and rounds it to an int |
| `LuaPopTable`, `LuaPopLightUserdata` | `0x1038b251c`, `0x1010de910` | check for a table (kept on the stack), pop a light userdata (nil gives null) |
| `FactionScriptInterface_GetArg`, `CharacterScriptInterface_GetArg` | `0x101d5c640`, `0x101d5bd18` | `(L, index, &out)`: a faction / character argument by stack index; the only script interface arguments in the campaign classes |
| `LuaPushString`, `LuaPushInt`, `LuaPushUInt`, `LuaPushFloat`, `LuaPushBool` | `0x1038b0b58`, `0x1038b0ddc`, `0x1038b0da0`, `0x1038b1b68`, `0x1038b00a8` | push a result |
| `LuaPushNewTable` | `0x1038b1fa0` | pushes a new table |
| per-class `Lunar::push` | e.g. `0x101c217dc` (faction), `0x101d52c44` (region list) | `(L, object, flag)`: nil for a null object, else a userdata with the class's metatable (looked up by name) |
| `State::operator<<(Pointer<T>)` | e.g. `0x1014a2224` (`TWUI::Component`) | a pointer userdata (a UI component's address) |

Lookups like `faction_by_key` push through a helper that pushes the object or a `NULL_SCRIPT_INTERFACE` with an error text; one level of such helpers is followed. The list classes are one template, `LIST_SCRIPT_INTERFACE<LIST, ITEM, OBJECT>` (`num_items`, `item_at`, `is_empty`); their tables are matched by the class name in the handlers' symbols.

## Dumping the game's Lua

`dump.lua` writes `TWASE/api_dump.tsv` in the game folder. Run it in a campaign, as a script mod: copy it to `data/campaigns/main_attila/mods/zz_apidump/scripting.lua`, load a save, wait for the file (a few seconds after the campaign starts loading), quit the game and remove the folder again. Copy the file to `data/api_dump_macos.tsv`.

The dump is usually taken with mods enabled (the saves need them). `apigen.py` keeps only what the vanilla scripts define and what the engine provides, so mod globals don't end up in the reference.

Findings from the dump (2026-10-13):

- The campaign's `scripting.lua`, the library it requires and the script mods run in an environment of their own, not `_G` (the game's `require` loads files into the caller's environment); `_G` only has Lua's standard library and a few engine functions (`CliExecute`, `RequireRegister`, `system`, `vfs`).
- Lunar methods have no upvalues, so a method can't be mapped to its table entry from Lua.

## Limits

- Optional or alternative arguments (mostly `UIComponent`): the arguments list every type the handler can read, not the alternatives.
- About 7% of the campaign methods have no type: mostly ones that take and return nothing, a few `UIComponent` methods and `CampaignCharacter`, `NULL_SCRIPT_INTERFACE`, `ScriptedValueRegistry` (no method table found).
- Battle and front end: not dumped yet (only the campaign environment); their tables are typed in the data, but the battle classes read their arguments differently (variadic), so their types are mostly missing.

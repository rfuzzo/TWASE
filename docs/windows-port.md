# Windows port of the Mac features (plan)

Status 2026-10-13: Windows is at the v1.0.0 features (console, Lua log, mod loader, unit size patch, deal score tweak), plus the shared core from `src/core` (config, console with the command queue). The Mac build has more, all of it needing addresses and struct layouts from the game code, which on Windows is in `empire.retail.dll` (TWASE resolves its addresses relative to that DLL's base, `App::GetEmpireDllAddr()`). This plan is for the PC side (IDA Home, x86).

Progress: steps 1-4 are done, step 5 partly (region_wealth and the DB tables; the record layouts are next) (branch `win/port-loose-mods-lua-api`). To port a `twase.*` function: add its addresses to `src/platform/win32/sdk/Attila/Addresses.hpp` under the Mac names, then set its flag in `src/platform/win32/sdk/Attila/Features.hpp` to 1. The engine's member functions are declared `TWASE_THISCALL` in `src/core/Scripting/LuaApi.cpp`; check each one's convention in IDA when porting it.

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
| Region trading (`twase.enable_region_trading`) | `Hooks/NativeRegionTrading.cpp` | 84 addresses and offsets: the region action's rules, the trade regions panel, the "Add Offer/Demand" list, deal terms, the evaluator and its components, the transfer |

Every Mac address is in [docs/macos/addresses.md](macos/addresses.md) with how it was found (strings, RTTI, callers); many entries also have the Windows RVA from v1.0.0 for comparison. `src/platform/macos/sdk/Attila/Addresses.hpp` has them all by name.

## Order

Each step works and can be tested on its own.

1. **Loose mods** (no RE). Done.
2. **Lua API**: the 11 functions. Done: `lua_Number` is `float` on Windows too. They're Lua 5.1 functions, so they're easy to find through `luaopen_base`, the only code that references the base library's `luaL_Reg` table (`"print"`, `"tonumber"`, ...): `luaB_tonumber` calls `lua_tonumber` / `lua_pushnumber`, `luaopen_base` calls `lua_pushcclosure`, `lua_createtable`, `lua_setfield`. Check whether Windows' `lua_Number` is `float` too (`lua_pushnumber` storing a single precision value): on Mac it is, and the API depends on it.
3. **Move `LuaApi.cpp` to `src/core`** (done, per-platform `sdk/Attila/Features.hpp` flags), so both platforms share the `twase.*` code. What it needs from the platform: resolving an address (Mac: `Image::Get()->GetSlide()`, Windows: `empire.retail.dll`'s base, `App::GetEmpireDllAddr()`), the safe read for the dev probes, and the same names in each platform's `Addresses.hpp`. Functions whose addresses Windows doesn't have yet can stay unregistered there (one `#if` or an `available` flag per function).
4. **`twase.faction_attitude`** (done: `FactionScriptInterface_GetArg` 0x80C480, `get_faction_attitude` 0xD38000 returns a float there): `FactionScriptInterface_GetArg` (the faction argument check of methods like `allied_with`) and `CAI_INTERFACE::get_faction_attitude`. On Mac the latter was found through the RTTI of its lambda (`…CAI_INTERFACE20get_faction_attitude…$_0`); MSVC names lambdas `<lambda_…>` without the function, so on Windows go through its callers instead: the diplomacy screen's attitude display, which buckets the value into 5 attitude levels.
5. **Regions and the database**: offsets change with the pointer size (a `CA::String` is 12 bytes on x86, 16 on arm64), so every record layout has to be found again: turn on `lua_dev_functions` and use `twase.dev_db_fields(table, index)` to dump records, then match them with the TSV export of the table (`twpack db export`).
   Status: `twase.region_wealth` and the table access are ported (addresses in `src/platform/win32/sdk/Attila/Addresses.hpp`). The record readers (`building_level`, `unit`, `levy_pool`, `region_province`) are behind `TWASE_HAS_DB_RECORDS`, still 0 on Windows. Found statically so far: `building_levels` records (`BuildingLevelRecord_ctor` 0xF02E40) are key `+0x0`, chain record* `+0xC`, then 4 ints from `+0x10` (level, create_time, create_cost, upkeep_cost on Mac); the other tables' raw readers branch on the file version (e.g. `MainUnitRawRow_read` 0x1053BB0), so their layouts come from the dumps.
6. **`twase.localise`**.
7. **Region trading**, last and largest. Port it in the order of the Mac commits (`git log --follow src/platform/macos/dylib/Hooks/NativeRegionTrading.cpp`): rules → panel → evaluation → Region for Troops. On x86 the engine's functions are mostly `__thiscall`, and struct returns go through a hidden pointer instead of arm64's `x8`.

## Finding things in `empire.retail.dll`

- **The campaign API is the same on both platforms**: same game scripts, same bindings. `tools/apigen/dump.lua` runs on Windows unchanged; diff its output with `tools/apigen/data/api_dump_macos.tsv` to confirm.
- **Lunar method tables** are plain pointer arrays in `.rdata` on Windows (no chained fixups): a name pointer and an MSVC member function pointer per entry (its size depends on the class's inheritance), and the method name strings lead straight to the handlers. The Mac handler addresses in `tools/apigen/data/lunar_types_macos.json` say which handler to compare.
- **Inlining differs**: MSVC and clang inline differently, so a function that's separate on Mac can be inlined on Windows (or the other way round). Then hook the caller, or patch at the call site.
- **Hooks**: Detours on Windows (`Hooking/DetourTransaction`), Dobby on Mac; the hook bodies are the same C++.

## Done when

- every `twase.*` function exists on Windows and gives the same results as on Mac (the tests in `docs/macos/addresses.md` list values to compare, e.g. Olisipo's wealth breakdown, `twase.region_province` for Olisipo → `att_prov_lusitania`)
- [lua-api.md](modding/lua-api.md) no longer says "Mac only"
- the region trading mod works on Windows

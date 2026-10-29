# Address table

Steam sections: Feral 1.6.1 RC2, `CFBundleVersion 480285.103778`, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326`. App Store RC4 has its own section at the end.

Addresses are **unslid VAs** as shown in Binary Ninja (image base `0x100000000`); runtime address = VA + ASLR slide.
Earlier mappings may also be named in the bndb; the campaign UI mapping below was read-only and did not rename or save the database.

## Lua

The binary contains **two** Lua 5.1 copies. The game scripting uses the one at `0x103f0xxxx`–`0x103f2xxxx` (base library `luaL_Reg` table at `0x104d84c70`). The other copy at `0x100e9xxxx` (table at `0x104b18698`) is Feral's; it is named `feralLua_*` in the bndb. Don't mix them.

| Name | PC RVA | Mac VA | Found via |
|---|---|---|---|
| luaL_loadbuffer | `0x012C7E20` | `0x103f137d8` | `luaB_loadstring`; verified `(L, buf, size, name)` → `lua_load` `0x103f2a35c` |
| lua_tolstring | `0x012C7380` | `0x103f283b8` | `luaB_print` |
| lua_settop | `0x012C7210` | `0x103f270f8` | `luaB_next` |
| lua_gettop | `0x012C67B0` | `0x103f270e8` | `luaB_pcall` |
| lua_getfield | `0x012C66D0` | `0x103f291d4` | `luaB_print` (`getfield(L, GLOBALSINDEX, "tostring")`) |
| lua_pcall | `0x012C6B00` | `0x103f2a100` | `luaB_pcall` |
| lua_next | `0x012C6A50` | `0x103f2a550` | `luaB_next` |
| lua_pushnil | `0x012C6CE0` | `0x103f28c80` | `luaB_next` |
| lua_pushvalue | `0x012C6D80` | `0x103f2755c` | `luaB_print` |
| lua_type | `0x012C74E0` | `0x103f27654` | `luaB_type` |
| lua_getmetatable | `0x012C6730` | `0x103f295bc` | `luaB_getmetatable` |
| lua_remove | `0x012C6F60` | `0x103f271a0` | `luaL_getmetafield` |

Also named: `lua_insert` `0x103f272a4`, `lua_pushboolean` `0x103f2903c`, `lua_pushstring` `0x103f28d48`, `lua_setfield` `0x103f29964`, `lua_call` `0x103f2a0a4`, `lua_rawget` `0x103f2931c`, `luaL_error` `0x103f11e80`, `luaB_print` `0x103f09540`.

## Lua log (PC `LuaLog` hook)

On Mac the Lua output goes through a global function pointer, the **sink**:

| Name | Mac VA | Notes |
|---|---|---|
| g_LuaLogSink | `0x1056d6588` | `void (*)(const char*)`; `print`, `out`, the error handler (`0x1038b1420`, `"Lua Error: [%s], %s\n%s\n"`) and `lua_writestring` `0x103f08c20` call it, or `fputs(stdout)` if it is null |
| SetLuaLogSink | `0x103f25de8` | `adrp x8; str x0, [x8, #0x588]; ret`. Called from both ScriptingEnv constructors (`0x102c40904`, `0x102c40e04`) |
| ScriptLog_NoOp | `0x1038af268` | the sink the game installs: a lone `ret` (logging stripped in retail) |

The sink function is a single instruction, so it can't take an inline hook. Instead:
1. Set `g_LuaLogSink` to our function at startup.
2. Patch the `str` in `SetLuaLogSink` at `0x103f25dec` to a `nop`, so the constructors can't reinstall the no-op. Expected bytes `00 c5 02 f9`, patch `1f 20 03 d5`.

## Script runtimes (PC `LuaGameEnvironment`)

| Name | PC RVA | Mac VA | Notes |
|---|---|---|---|
| GetLuaState | `0x1626AA0` | `0x1038b377c` | `lua_State* (void* scriptInterface)` = `GetScriptRuntime(this)->L`. Lazily creates the runtime, like PC |
| GetScriptRuntime | – | `0x1038b3794` | |
| RegisterScriptRuntime | – | `0x1038b261c` | creates `g_RuntimeLuaState` on first use and appends a `RuntimeLuaNode`. Also registers the `out` global |
| g_RuntimeLuaListHead | `0x01DA4230` | `0x10560b7c8` | `RuntimeLuaNode*` (first node, or `&sentinel` when empty) |
| g_RuntimeLuaListSentinel | `0x01DA4234` | `0x10560b7d0` | sentinel node; iterate `node = *head; while (node != sentinel) node = node->next` |
| g_RuntimeLuaState | `0x0291DE00` | `0x10560b7c0` | master state (unused by TWASE) |
| ScriptInterface_ctor | – | `0x1038b3690` | |
| g_ScriptingEnvArray | – | `0x105603548` | `{int capacity; int count; ScriptingEnv** data}`, filled by the ScriptingEnv constructors (not used yet) |

64-bit layouts (from the code above):

```cpp
struct ScriptInterface { void* vtable; ScriptRuntime* runtime; lua_State** parentState; uint64_t field_18;
                         void (*registerBindings)(lua_State*); uint32_t owned; };      // 0x30
struct ScriptRuntime   { lua_State* L; bool initialized; void* scriptInterface; uint64_t owned;
                         uint64_t field_20, field_28; };                                // 0x30
struct RuntimeLuaNode  { RuntimeLuaNode* prev; RuntimeLuaNode* next; ScriptRuntime* runtime;
                         lua_State* baseL; int registry_ref; };                          // 0x28
```

## Game loop (macOS only)

Threads: Feral runs CA's `WinMain` on a secondary thread named `WinMain`, and **all game Lua runs on it**. Metal frames are presented from a separate render thread, AppKit events arrive on the main thread.

| Name | Mac VA | Notes |
|---|---|---|
| GameTick | `0x1010ecd24` | `bool (void* app, int, int, int, int)`, called once per frame by `GameRunLoop`; returns true to stop. TWASE runs queued console commands after it |
| GameRunLoop | `0x1010ca7bc` | frame limiting + `GameTick` loop |
| GameOuterLoop | `0x1010c9c74` | `"Init completed"` → `GameRunLoop`, repeated |
| GameMain | `0x1010ce77c` | |
| WinMain | `0x1032bc10c` | `int64 (void* instance, void* prev, const char16_t* commandLine)`, `"Empire.CONFIGURATION_NAME.x64.dll"`. TWASE hooks it to log the command line |
| BuildCommandLine | `0x100022b7c` | Feral: appends `gfx_fullscreen`, `x_pos`/`y_pos`, `x_res`/`y_res`, then the mods |
| AppendModsToCommandLine | `0x100014f1c` | Feral: unless `DisableAllMods`, appends `mod <pack>;` for each enabled mod |
| AppendCommandLine | `0x100c36ccc` | appends a statement (space separated) to the global command line string `0x10506c7b0` |

Feral builds the engine command line itself; process arguments are not part of it. Feral's preferences (`~/Library/Application Support/Feral Interactive/Total War ATTILA/Preferences Data`, XML, edit only while the game is closed) control it instead (verified 2026-10-04):

| Preference | Effect |
|---|---|
| `GameOptionsDialogShouldShow` = 0 | skips Feral's pre-launcher (game options dialog / mod manager), the game starts directly |
| `ExtraCommandLineEnabled` = 1 + `ExtraCommandLine` = `"<statements>"` | appended at the end of the engine command line (`0x100c36624`), e.g. `mod <pack>;`, `game_startup_mode campaign_load <save>` |
| `DisableAllMods` = 1 | Feral appends no `mod` entries of its own |
| `Launcher/mods` | Feral's mod list: `<pack> = <timestamp>|<enabled>|<order>` |

With mods enabled, Feral links every subscribed Workshop pack into `VFS/Local/mods` (lowercased names) on each launch, which the game sees as `<install>\mods`. With `DisableAllMods = 1` it empties that folder, so `mod` entries from `ExtraCommandLine` only find packs in `TotalWarAttilaData/data` (where `twase-launch.command --mods` links them).

Feral's mod manager (`CModManager`, `Feral Interactive/Total War ATTILA/VFS/Local/…` on disk) lists Steam Workshop subscriptions **and local packs**: `VFS/Local/local_mods/<folder>/<name>.pack`, one folder per mod (a pack directly in `local_mods` is skipped, which is why the first test on 2026-10-13 failed). The entries go into `Launcher/mods` like Workshop items (key `<name>.pack`, time 0), with the on/off switch and the load order; on launch Feral links each into `VFS/Local/mods` and appends `mod <name>.pack;`. `stat` follows symlinks, so the pack (or the folder) can be a link to a build output. Tested 2026-10-13 with `region_trading.pack` and `fireforged_fixes.pack` linked from `TotalWarAttilaData/data`: both enabled from the preferences, linked, on the command line, opened by the game, kept when Feral rewrote its preferences; the launcher lists them by title (`region_trading`) with a drive icon instead of Steam's, switched on, with the load order arrows.

| Name | Mac VA | Notes |
|---|---|---|
| CModManager_ctor | `0x100012024` | folders: `local_mods` `+0x30`, `mods` `+0x70`, `mods/staging` `+0x90`; entries `std::vector` at `+0x18`/`+0x20`; `+0xb1` preference `AllowMultiplePacksInModFolder`; starts the refresh thread `0x1000154e4` |
| CModManager_Refresh | `0x100012c60` | clears the entries, then scans `local_mods` (`0x100012d2c`), subscribes to the Workshop items' details (`0x100012ddc`, `ISteamUGC` vtable `+0x208`/`+0x210`/`+0x128`) and reads `Launcher/mods` (`0x1000130bc`, `"%d|%d|%d"` → time, enabled, order) |
| CModManager_AddFolder | `0x100018e8c` | callback for each child of `local_mods`: must be a folder; takes its `*.pack` files (only the first unless `AllowMultiplePacksInModFolder`), or converts a `*.bin` into `mods/staging`; builds the entry (`0x10001afe0`) and appends it unless one with the same title exists |
| ModEntry_InitLocal | `0x10001afe0` | `+0x00` file name (preferences key, `mod` line), `+0x10` time 0, `+0x18` title (file stem without leading `@`), `+0x28` description (`Startup.ModManagerErrNoDataAvailable`), `+0x38` full path, `+0x48` parent folder name (several packs in one folder), `+0x58` order (-1), `+0x68` local = 1, `+0x69` enabled |
| CModManager_OnUGCDetails | `0x1000137dc` | `CCallResult<CModManager, SteamUGCRequestUGCDetailsResult_t>`: Workshop entries (`0x10001af70` from `SteamUGCDetails_t`, `0x1000147a0` `.bin` → `.pack`), `+0x68` = 0 |
| CModManager_FindByName | `0x1000141c0` | by file name or name + `.pack` (`0x100014084`: by title) |
| CModManager_SortByOrder | `0x1000146d8` | sorts the entries by order and closes gaps; only the launcher UI calls it (`0x10001e61c`) |
| CModManager_SavePrefs | `0x10001448c` | writes `Launcher/mods` from the entries |
| CModManager_LinkPacks | `0x100014b34` | symlinks each entry's pack into `mods/<lowercased name>` if not there yet |
| FeralAppendModsToCommandLine | `0x100014f1c` | unless `DisableAllMods`: refresh if needed, wait **at most 1 s** for the Workshop details, link, then `mod %s;` for each enabled entry **in vector order** |
| ModManagerUI_GetProperty | `0x10001b88c` | answers the launcher UI (names are `NString` globals from `0x105017ff8`, e.g. `ModManagerAllModInfo` `0x105018078`); `ModManagerIsWorkshopItem` = `!(entry + 0x68)` |

Without the launcher UI (`--skip-launcher`) nothing sorts the entries, so the `mod` lines come in vector order: local packs first, then the Workshop items in the order Steam answered (not the launcher's order), and Workshop items whose details take longer than 1 s are missing: the preferences (on/off, order) are only applied once the last details came in (`+0x10` count of pending requests, then `+0x9` refreshing = 0). TWASE's `FeralModOrder` hook on `CModManager_LinkPacks` fixes both: it waits up to 15 s more for `+0x9`, then calls `CModManager_SortByOrder` (enabled first, then by order, as the launcher does before PLAY) and logs the result (`Feral mod manager: enabled mods in load order: …`). LinkPacks only runs from `FeralAppendModsToCommandLine` after the `DisableAllMods` check, so `--mods` launches don't wait. Tested 2026-10-13: two `--skip-launcher` launches gave the preferences' order (Steam answered within Feral's 1 s, so the extra wait wasn't needed). (`TWASE/local_mods.txt`, an earlier workaround that added local packs through `ExtraCommandLine`, is gone.)

## Patches

| Name | PC RVA | Mac VA | Found via | Notes |
|---|---|---|---|---|
| BitSetCrashAddr (unit size patch) | `0x0091CB57` (`cmp edi,40h; jnb`) | `0x101a51c58` (`b.eq`) | xref to `"bitset test argument out of range"` (5 sites). Only this one has the `mov x8,#-1` mask → vcall(vtable+0x30, &mask) → list walk → `cmp x20,#0x40` shape | expected `40 26 00 54` (`b.eq 0x101a52120` → throw). Patch `80 00 00 54` (`b.eq 0x101a51c68`, include branch) |

## Lua environment classes (from RTTI)

The word before each vtable points to the class's `type_info`, which gives the real class names. All game Lua environments derive from `UTILITYDLL::LUA::LuaEnv` (the PC "ScriptInterface"), whose virtual slot 3 (pure) loads and runs the environment's startup script:

| Class | vtable | Slot 3 | Notes |
|---|---|---|---|
| `UTILITYDLL::LUA::LuaEnv` | `0x104d14c90` | pure | base, 0x30 bytes, ctor `0x1038b3690` |
| `EMPIRECAMPAIGN::EPISODIC_SCRIPTING_ENV` (+ `EMPIREUTILITY::KEY_CONTROLLED`) | `0x104bfa178` | `0x102170104` | the PC "ScriptingEnv": campaign `scripting.lua`, 0x78 bytes, ctor `0x10216fe40` |
| `EMPIREUTILITY::EMPIRE_LUA_ENV` | `0x104c53b98` | `0x102c417c8` | `"EmpireLuaEnv"`, ctors `0x102c40904` / `0x102c40e04` install the log sink |
| `FRONTEND::AUTORUN_SCRIPTING_ENV` | `0x104c6a470` | `0x10314d060` | frontend autorun |
| `EMPIREBATTLE::BATTLE_EDITOR_SCRIPT_INTERFACE` | `0x104b5d568` | `0x10149e0c0` | |
| `EMPIRECAMPAIGN::CAMPAIGN_UI_SCRIPT_INTERFACE`, `UIDLL::FRONTEND_UI_SCRIPT_INTERFACE` | `0x104cd3de8`, `0x104cde8b8` | stub `0x100023188` | bindings only |

`EPISODIC_SCRIPTING_ENV` layout: `LuaEnv` `+0x00` (0x30), `KEY_CONTROLLED` `+0x30` (0x28, own vtable, offset-to-top −0x30), folder `+0x58` (`"campaigns/main_attila"`), script `+0x68` (`"campaigns/main_attila/scripting.lua"`). Strings are `{u32 length, u32 capacity, char* data}`. Same sequence as on Windows (0x18 + 0x1C + 0xC + 0xC = 0x4C), where the two path fields were labeled the other way round.

## Mod loader (PC `RunStartupPath` hook)

| Name | PC RVA | Mac VA | Notes |
|---|---|---|---|
| RunStartupPath | `0x79B980` | `0x102170104` | `void* (EPISODIC_SCRIPTING_ENV*)`, vtable slot 3; reads the script through the VFS, sets `package.path`, `luaL_loadbuffer`, runs it, names the state `"EpisodicScriptingEnv"`. Found via `"campaigns/%S/scripting.lua"` (`0x102182814`, `0x102184538` build the paths and construct the env) |
| VFS_GetInstance | `0x1658C80` | `0x10387c0a8` | returns the static VFS object `0x1055b84e0` |
| VFS_SearchFiles | `0x1635B50` | VFS vtable `+0x58` | `(vfs, const CName* dir, const CName* pattern, VFSSearchResults*, flags, 3)`; dir/pattern **by reference** (PC: pooled `char*` by value). The game passes flags 0 for one folder; TWASE uses 1 (with sub folders) |
| CName_ctor | `0xDF290` | `0x10109240c` | `(CName*, const char*)` |
| tw_free | `0xE98C0` | `0x1010acb70` | game allocator free (`tw_malloc` `0x1010aca70`) |

`VFSSearchResults` = `{u32 capacity, u32 count, VFSEntry** entries}`; `VFSEntry` has the UTF-8 path at `+0x08` (backslashes, e.g. `campaigns\main_attila\mods\x\scripting.lua`).

Once a loaded pack contains `campaigns/<campaign>/mods/`, the search returns only that pack's entries, not the loose files in `TotalWarAttilaData/data` under the same folder (tested 2026-10-11 with `fireforged_fixes.pack` and a loose `region_trading`). Loading a found file still works through the VFS (`require`). The mod loader therefore also scans the loose folder on disk.

Tested 2026-10-04:

| Mod location | Result |
|---|---|
| loose: `TotalWarAttilaData/data/campaigns/main_attila/mods/<name>/scripting.lua` | ✅ found and loaded |
| inside a loaded `.pack`: `campaigns\main_attila\mods\<name>\scripting.lua` | ✅ found and loaded |
| Feral's user folder `…/VFS/User/AppData/Roaming/The Creative Assembly/Attila/maps/…` | ❌ not searched by the VFS, and the game **empties that folder on every launch** |

The game's `require` names chunks `q:\feral\users\default\appdata\roaming\the creative assembly\attila\maps\<path>` (it looks in that user folder first), but the content comes from the VFS (data / packs).

## Diplomacy deal score (PC `DiplomacyLikelihood`)

| Name | PC RVA | Mac VA | Found via |
|---|---|---|---|
| CAI_GetDealLikelihoodBucket | `0xD2A7A0` | `0x102296a5c` | the only reader of the tweakers `CAI_DIPLOMACY_NEGOTIATION_DISPLAYED_LIKELIHOOD_THRESHOLD_LOW/HIGH` (objects `0x105600378` / `0x1056003e0`, created in `mod_init_func_1061`, float value at `+0x64`, defaults -4 / 4). `int (caiModule, deal)`, returns 2 low / 3 moderate / 4 high |
| Diplo_GetDisplayedDealScore | `0xA6E980` | `0x101d7e890` | called by the bucket function: `float (deal, *(cai + 0x788), factionA, factionB)`, thin wrapper around the evaluator `0x101d7b25c` |
| DiplomacyDropdown_SetLikelihood | `0x14F95A0` | `0x1035f4a1c` | sets `dy_chance`'s state to `"low"` / `"moderate"` / `"high"`, -2 hidden |
| UIComponent_SetTooltipText | `0x13B9A00` | `0x1032d2418` | the `SetTooltipText` Lua binding (`0x1032f9188`, UIComponent binding table at `0x105001120`) |
| UIComponent_SetState | – | `0x1032d1b08` | |
| UIComponent_FindChild | – | `0x1032d4f88` | `(parent, name, recursive)` |
| String_ctor / String_dtor | – | `0x10104ce1c` / `0x10104ba58` | `CA::String` from UTF-8 |
| String_ToUniString | (PC: `WString_ctor` `0xDFEF0`) | `0x101049bb0` | `CA::UniString` **returned by value through x8**; TWASE declares the return type with a non-trivial destructor so clang uses x8 too |
| UniString_dtor | `0xE0720` | `0x101049d20` | |

| Offset | PC | Mac | Found via |
|---|---|---|---|
| DiplomacyDropdown → `dy_chance` | `0xBC` | `0x160` | DiplomacyDropdown init `0x1035ec3a0` (`tx_likelihood of success` at `0x158`) |
| UIComponent → current UIState | `0xB4` | `0x140` | `GetTooltipText` / `CurrentState` Lua bindings (`0x1032f9384` / `0x1032f8fd4`) |
| UIComponent → tooltip | `0x118` | `0x1c0` | same |
| UIState → tooltip | `0x44` | `0x60` | same; state name at `0x20` |

`CA::UniString` = `{u32 length, u32 capacity, char16_t* data}`, the data has an allocation header in front.

Tested 2026-10-04 (`twase.*`): `faction_attitude` matches the diplomacy screen (a's attitude towards b), `region_wealth` matches the province panel and its wealth breakdown tooltip (e.g. Olisipo 373 = farming 33 + fertility_agriculture 164 + sea_trade 176; the UI calls them Agriculture, Agriculture [Fertility], Maritime Commerce).

Tested 2026-10-04: the "Likelihood of success" tooltip shows the deal score (e.g. -12.35 → low, 0.71 → moderate); forced likelihoods (gifts) get no score.

## Lua API for mods (`twase.*`, `src/platform/macos/dylib/Scripting/LuaApi.cpp`)

More of the game's Lua 5.1 API, for registering C functions. **The game's `lua_Number` is `float`** (`lua_pushnumber` stores `s0`), not `double`.

| Name | Mac VA | Found via |
|---|---|---|
| lua_pushnumber | `0x103f28c94` | `luaB_tonumber` |
| lua_tonumber / lua_isnumber | `0x103f280a0` / `0x103f27850` | `luaB_tonumber` |
| lua_pushcclosure | `0x103f28e80` | `luaopen_base` (`0x103f08c44`, the only code referencing the base `luaL_Reg` table) |
| lua_createtable / lua_setmetatable | `0x103f2954c` / `0x103f29d64` | `luaopen_base` |
| lua_pushlstring / luaL_register | `0x103f28cd8` / `0x103f12950` | `luaopen_base` |
| lua_touserdata | `0x103f28874` | the game's faction argument check below |
| lua_rawequal / luaL_typerror | `0x103f27b7c` / `0x103f11f6c` | same |
| lua_topointer | `0x103f28a60` | `luaB_tostring` |

## Campaign script interfaces (Lunar bindings)

The campaign model is bound to Lua with the **Lunar** template (`Lunar<T, LIST_POOL_ALLOCATOR<T>>::thunk` etc. are named). A Lua object is a userdata holding `T*`; the metatable is in the registry under the class name (`"FACTION_SCRIPT_INTERFACE"`). Each class has a static method table of 24-byte entries `{const char* name, method, this_adjust}`, found through the method name strings (e.g. `"allied_with"` → entry `0x104f83318`); `FACTION_SCRIPT_INTERFACE`'s table runs from `0x104f831b0` (`region_list`) to `0x104f83570` (`is_trading_with`). Methods take `(T* this, lua_State* L)`. Data pointers in `__DATA` are chained fixups (`DYLD_CHAINED_PTR_64_OFFSET`: target = `0x100000000 + (raw & 0xFFFFFFFFF)`).

| Name | Mac VA | Notes |
|---|---|---|
| FactionScriptInterface_GetArg | `0x101d5c640` | `bool (L, idx, FACTION_SCRIPT_INTERFACE** out)`: 0 if not a userdata, `luaL_typerror` for other userdata types |
| FACTION_SCRIPT_INTERFACE::allied_with | `0x101d2b344` | reads the relationship from `FACTION + 0x930` (hash map keyed by the other faction's id at `FACTION + 8`) |
| FACTION::is_dead | `0x101b65694` | `*(FACTION + 0x8f8) == 0` |
| CAI_INTERFACE::get_faction_attitude | `0x10229d814` | `int (CAI_INTERFACE*, const FACTION& a, const FACTION& b)`, sums the attitude components through a lambda (`std::function` vtable `0x104c12268`, RTTI `…CAI_INTERFACE20get_faction_attitude…$_0`) and rounds. Callers `0x101cf4148` / `0x101cf4368` (the latter buckets it into 5 attitude levels) |

| Offset | Mac | Notes |
|---|---|---|
| FACTION_SCRIPT_INTERFACE → FACTION* | `0x10` | |
| FACTION → model handle | `0x18` | `*(handle) + 0x48` = campaign world (`0x101e777bc`) |
| world → CAI_INTERFACE* | `0x2d60` | |
| REGION_SCRIPT_INTERFACE → REGION* | `0x10` | `owning_faction` reads `**(REGION + 0x1a0)` |
| REGION → wealth | `0x16c` | int, the region's wealth (GDP before tax) in the province panel (`0x1034cd5bc`) |
| REGION → wealth breakdown | `0x7a0` | 15 × `{int base, int total, float bonus_percent}`, passed as the data of the `wealth_breakdown_tooltip` (type 4, `0x1034d2444`). Category order = the loc key table `campaign_localised_strings_string_gdp_*` at `0x104bc7590`: subsistence, animal_husbandry, farming, local_trade, land_trade, sea_trade, entertainment, learning, mining, manufacture, other, fertility, fertility_agriculture, fertility_livestock, poor_fertility_bonus |

`REGION_SCRIPT_INTERFACE`'s method table: `0x104f83f60` (`model`) … `0x104f84158` (`sanitation`); `town_wealth_growth` reads `REGION + 0x184`, `region_wealth_change_percent` `REGION + 0x174`. There's no region argument check like the faction one, TWASE does its own (type, registry metatable `"REGION_SCRIPT_INTERFACE"`, `lua_rawequal` `0x103f27b7c`).

### Argument and push helpers (named in the bndb 2026-10-13)

Campaign handlers get a context `ctx = LuaGetLookup(L)` (`ctx[0]` the `lua_State*`, byte at `ctx + 8` still valid) and pop their arguments from the top of the stack (so in reverse order); a type mismatch calls `LuaTypeMismatchError` and clears the valid flag.

| Name | Mac VA | Notes |
|---|---|---|
| LuaPopInt / LuaPopInt2 | `0x1038b0a68` / `0x1038b2318` | `(ctx, int32* out)`: number, rounded (two identical copies) |
| LuaPopTable | `0x1038b251c` | `(ctx, ref)`: checks for a table, keeps `ctx` and the stack top in `ref` |
| LuaPopLightUserdata | `0x1010de910` | `(ctx, void** out)`: light userdata, nil gives null |
| LuaPushNewTable | `0x1038b1fa0` | `lua_createtable(L, 0, 0)` into a table reference |
| FactionScriptInterface_GetArg / CharacterScriptInterface_GetArg | `0x101d5c640` / `0x101d5bd18` | `bool (L, index, T** out)`, by stack index: the only script interface arguments in the campaign classes |
| LunarPush_`<CLASS>` | 24 functions, e.g. `0x101c217dc` (faction), `0x101d52c44` (region list) | `(L, object, flag)`: nil for null, else a userdata with the metatable `"<CLASS>"` from the registry; flag 0 also marks it "do not trash" |
| LuaPushComponentPointer | `0x1014a2224` | `State::operator<<(Pointer<TWUI::Component>)`: a UI component's address |

The list classes are one template, `LIST_SCRIPT_INTERFACE<LIST, ITEM, OBJECT>::{num_items, item_at, is_empty}`, with weak definitions: their method table entries are binds to the binary's own symbols, not rebases. Each method table starts with the virtual `is_null_interface` (member function pointer with bit 0 of the adjustment set, vtable offset `0x38`). The tables and types are in `tools/apigen/data/lunar_types_macos.json`.

## Database (loaded DB tables, game + mods merged)

Reached from the campaign global the UI uses: world = `*(*(*0x105608ea0 + 0x30) + 0x48)`, DB container = `DB_GetContainer(world + 0x21e0)` (`0x101e76780`: `*(*(*field + 0x58) + 0x20)`). Each table has a lazy accessor `DATABASE_TABLE* (container)` that loads it on first use (`"Loading database: %s"`, the name string `<table>_table` is referenced only by its accessor) and caches it at a fixed container offset. Lookup by key: `StringHashMap_Find(table + 0x18, const TempString* key)` (`0x1010e5040`) → `int* index`, records at `table + 0x10` (`RECORD**`), count at `table + 0x0c`. Records start with their key (`TempString` at `+0`) and follow the schema's column order; references to other tables are record pointers. Junction tables have composite keys, so they're scanned. Layouts found by dumping records in game (`twase.dev_db_record`) and matching the extracted TSVs.

| Table | Accessor | Container offset | Fields |
|---|---|---|---|
| building_levels | `0x102c86dfc` | `0x1fb0` | `+0x10` chain record*, `+0x18` level, `+0x1c` create_time, `+0x20` create_cost, `+0x24` upkeep_cost (int32) |
| main_units | `0x102c90940` | `0x14d8` | `+0x30` create_time, `+0x40` num_men, `+0x48` multiplayer_cost, `+0x68` recruitment_cost, `+0x88` upkeep_cost, `+0xb8` unique_index |
| region_to_province_junctions | `0x102cc5c08` | `0x1630` | 0x20-byte records: `+0x00` province key, `+0x10` region key (TempStrings) |
| provinces | `0x102c8ea88` | `0x1638` | key at `+0`, record built by `0x10298a390` (key + loc entry at `+0x10`) |
| faction_to_mercenary_set_junctions | `0x102cc6b40` | | 0x10-byte rows: `+0x00` faction record*, `+0x08` mercenary pool record* (a people's levy pool, 15 factions) |
| mercenary_pool_to_groups_junctions | `0x102cc71b8` | | 0x38-byte rows: `+0x00` id, `+0x08` pool record*, `+0x10` unit group record*, `+0x18` initial_unit_count |
| mercenary_unit_groups | `0x102cc6e98` | | 0x28-byte records: `+0x00` chance_to_replenish (float), `+0x08` key, `+0x18` max_count, `+0x1c` max_replenish_per_turn, `+0x20` main_units record* |

`REGION + 0x78` = its `regions` record (the Lua `name()`), `REGION + 0x4b0` → the campaign province object (`**`), whose per-faction parts are at `+0x94` (count) / `+0x98` (`sub_102410864`).

Tested 2026-10-04 (unattended runs with Fireforged): `twase.building_level`, `twase.unit` and `twase.region_province` return the TSV / UI values (e.g. Olisipo → `att_prov_lusitania`).

## Localisation

The loc manager is at `DB container + 0x2200` (`sub_1030f0534`). Its virtual `+0x30` is the lookup: `const WString* (loc, const TempString* key, int)` (callers pass 1), returning the UTF-16 text (`CA::UniString`) or an empty / null string for a missing key. Used e.g. by `0x10298a390` (`provinces_onscreen_<key>`) and `0x101cb17c0` (the GDP category names). Tested 2026-10-04: `twase.localise` returns region, province, faction and UI strings; missing keys give nil.

## Native region trading (Rome II leftover)

Attila keeps the whole region trading feature: the diplomatic action type 6 (`EMPIRECAMPAIGNAI::DIPLOMATIC_ACTION_TRADE_REGIONS`, vtable `0x104c1e520`, its slot 13 `0x1025c8188` executes the deal and transfers the regions), the permission "regions" (`force_diplomacy` id 4; action type → permission table at `0x10466c5a0`, 2-bit permissions at `relationship + 0x90`, setter `0x101cf7a40`, getter `0x101cf79e4`), the game command `CCQ_DIPLOMACY_PROPOSE_TRADE_REGIONS` and the UI (`diplomacy_hud`: `offer_button_trade_regions` / `DiplomacyListTradeRegionsButton`, `trade_regions` / `DiplomacyTradeRegionsPanel`, loc "Gift Region").

The negotiation creates the action but never marks it available (`+0x18`), so the "Add Offer/Demand" list (built by `0x1036cca88(context, offers, demands)` from the available offer-side actions, context = `DiplomacyDropdown + 0x200`, negotiation = context `+ 0xd0`) never shows it. The actions are at negotiation `+0x710` (count `+0x70c`), only the campaign's types, not indexed by type. Action layout: `+0x8` negotiation, `+0x10` side (1 offer), `+0x14` type, `+0x18` available, `+0x19` blocked; the region action keeps its offered / demanded regions at `+0x20/+0x24/+0x28` and `+0x30/+0x34/+0x38`.

It's the vanilla "Gift Region" for settling hordes: the availability rule `0x1025c7a6c(a, b)` needs one side human and not a horde and the other a horde (FACTION `+0x970` is_human, `0x101b521ec` is_horde) that can own land (FACTION `+0x1751`, getter `0x101bec634`, also checked by `transfer_region_to_faction`; 0 e.g. for the Huns) and is not at war with it (the relation `0x101cc7864(a + 0x930, b) + 0x18` → `+0x18`, non-zero at peace, as in `at_war_with` `0x101b6125c`), and the region rule `0x101bdad20(from, to)` (returns through x8) lists `from`'s regions except the capital (`FACTION + 0x9d0`) and settlements with a garrisoned army / navy, and with `to` only regions where an army of `to` stands. The panel (`0x1035ad948`) fills `list_offers` with (player → AI) and `list_demands` with (AI → player), i.e. demands are the AI's regions where the player's army stands.

`src/core/Hooks/NativeRegionTrading.cpp` (switched on by the mod with `twase.enable_region_trading{ value = fn, transfer = fn }`) offers the region action to a horde that can own land also at war (vanilla's region list, the entries wait for peace in the deal) and adds Region for Troops for it; it widens both rules for a human and a settled (non-horde) AI: available if the human has a region to give, and both lists (offers and demands) show every region of the giving side but the capital and garrisoned ones. The panel hides `list_demands` in the layout and the fill function (`0x1035adce8(panel, list, from, to)`) skips invisible lists, so the hook shows it before the original populates the panel. Tested 2026-10-05 (WRE → ERE): all ERE regions can be demanded, Serdica alone scores −6.2. Tested 2026-10-04 by hand (WRE → ERE): "Gift Region" is in the list, the region appears under "Your Offers", the AI accepted and the region changed hands immediately. The deal score shows +0.0 for a region: the evaluator gives it no value (no region component in the deal evaluation tables).

### Panel, deal list and transfer (2026-10-07)

- Panel clicks `0x1035ae4c4(panel, event)`: `event[0]` the clicked component (OK / cancel by id, or a row's checkbox, its row at `+0x108`), the byte at `event + 8` is 1 if the checkbox is now ticked (only that byte: the rest of the word is garbage). It calls `SelectRow 0x1035adff8(panel, row or null)`, which unticks every row of both lists and keeps one selection (`panel + 0x78`). TWASE handles row clicks itself and keeps any number of ticked rows.
- OK: `0x1036cb868(context, regionId, demandedFrom, deal)` builds deal terms (`0x1024c8130`, offered regions `0x1025c7f00` → terms `+0x20`, demanded `0x1025c7df4` → `+0x10`) and applies them (`0x1025d2124`, the deal's virtual `+0x78`); TWASE puts all ticked regions in.
- The action (vtable `0x104c1e520`) keeps a display string per list (`+0x40` list 1, `+0x50` list 2, built in slot 14 `0x1025c82d4` from loc `offer_or_demand_regions`), but the deal list row (`DiplomacyListTradeRegionsButton`, setup `0x1035a6eec`) shows the demanded regions if there are any, else the offered ones, in both columns; TWASE rewrites `region_names` per column (regions the local faction owns in `your_offers`).
- Execute (slot 13 `0x1025c8188`): list 1 to negotiation faction A, list 2 to B, each through `0x101ceb290(settlement = region + 0x1a8, faction, 0, 0)` → `0x101ce748c(…, reason 5, …)`. The Lua `transfer_region_to_faction` (`0x10216f6c4`) uses `0x101cec078` (reason 11, then `0x101ce7d5c` for garrisons). Both update the minimap at once (tested with `twase.dev_transfer_region`, a probe registered with `[dev] lua_dev_functions = true`). Transferring a province capital kills its governor; TWASE calls the mod's `transfer` handler before / after so it can protect him.
- Region owner: `**(region + 0x1a0)` (the Lua `owning_faction`), localised name `0x101c57700(region)`.
- Deal scores: the gift table returns 0 for amounts small relative to the AI's treasury (a swap 439 short scored 0 for the ERE); TWASE interpolates linearly below the first non-zero step.

### Region for Troops (2026-10-07)

- **The "Add Offer/Demand" list:** entries in a vector at `DiplomacyDropdown + 0x1c0` (`{ capacity, count, ENTRY* }`, 0x20-byte entries: UniString text = the button's tooltip, two flags at `+0x10` / `+0x11`, the action at `+0x18`), built by `0x1036cca88(context, offers, demands)`. `0x1035f4538(dropdown)` makes a button `diplomac_action_possible_<type>` under `dropdown + 0x118` (template `dropdown + 0x120`) for each entry whose action isn't in the deal; `0x1035a9880(controller, entry)` stores the action at controller `+0x50`, the label `row_tx` from the action's name. A second button for an action: push a copy of its entry (`0x103200f00`, copies the UniString) before the engine makes the buttons, take the second button with that id, remove the copy afterwards. Renaming a component's id (TempString at `+0xf8`) doesn't change what the engine's Find returns, so we keep the id.
- **A click on an action button:** `0x1035f4fe8(dropdown, event)` (and `0x1035f5290`, the same through the dropdown's other base, `this = dropdown + 0x68`): `event + 8` the button, `event + 0x10 == 1` a click. Then the dispatcher `0x1035f5044(dropdown, 0, action)` by type: 6 → the region panel (`0x1035f6b88`); the "simple" types (mask `0x8881011`: 0, 4, 12, 19, 23, 27, e.g. peace) → `0x1036cbadc(context, action)` (empty terms, then the deal's apply); 25 (tributary) → `0x1036cba5c(context, action)`, which sets the direction in the terms (`0x1024d5b54`: the other side becomes your tributary; with empty terms the game makes *you* the tributary). Context = `*(dropdown + 0x200)`, its negotiation at `+0xd0`.
- **A faction's regions:** FACTION `+0xa4c` count, `+0xa50` the list (the Lua `region_list` `0x101d2a614`). The engine offers "Make Tributary State" only to a faction with land; `cm:force_make_vassal(master, vassal)` makes a puppet, and the scripts have no call for a tributary.
- **Action types** (button ids): 0 trade agreement, 1 cancel trade, 4 military alliance, 5 cancel alliance, 6 trade regions, 9 payments, 10 create puppet state, 12 peace treaty, 13 declare war, 14 join war, 15 break trade agreement, 16 break military alliance, 19 non-aggression pact, 21 military access, 22 cancel military access, 23 defensive alliance, 25 make tributary state, 28–31 break / cancel pacts and puppet status. At war only peace is offered; once peace is in the deal the others (tributary too) follow, but the action's available flag (`+0x18`) updates later than an add in the same call.
- **The deal list row's heading** (`DiplomacyListTradeRegionsButton`): `diplomacy_button / button_tx`.
- **Deal evaluation components:** the entry index is the action's position in the negotiation's action array (the evaluator switches over that index: position 6 is Payments, type 9), so a deal's peace / tributary components are found through `actions[index]`'s type.
- **Gift size:** a gift's linear score part `0x101d76bb4` (vtable `+0x28`) is the amount within / beyond the AI's budget, weighted by its personality, over the paying faction's GDP (`0x101b70018`), capped; the size class (vtable `+0x20`, `0x101d7ef3c`) is small / medium / large from `gift_base_value_*` (500 / 2,000 / 7,500) × the economic power `0x1019e1568(cai, treasuryOffset)` = `clamp(GDP / economic_power_gdp_factor + (treasury + offset) / economic_power_treasury_factor, min, max)` (8,000 / 10,000 / 0.2 / 4) of the gift's second faction. Treasury: `0x101cb57f4(FACTION + 0x250)`. The gift's score parts: vtable `+0x10`, `+0x18` (a table value, needs the scorer's context), `+0x20`, `+0x28`, `+0x30`, summed by `0x101d7e26c`.
- **Garrisons** (Lua): a settlement's garrison is a military force with a "general" that is a `colonel` (or the province's governor, a `general`) and costs no upkeep (`force:upkeep() == 0`); `has_garrison_residence()` is true for any army inside a settlement.
- **Levy:** the recruitment panel's sources (`0x1034676c8`) offer "Recruit from Horde" when a source faction is a horde (`0x101b521ec`); "Levy from tributary" / "Levy from puppet state" are the other sources of the same `LEVY_MERCENARY_MANAGER`. A people's own pool: `faction_to_mercenary_set_junctions` → `mercenary_pool_to_groups_junctions` → `mercenary_unit_groups` (see the DB table above, `twase.levy_pool`).

### Region value in the deal evaluation

`Diplo_EvaluateDeal` (`0x101d7b25c`, `(negotiation, aiContext, CAI faction a, CAI faction b, out, out)`) builds one `CAI_DEAL_COMPONENT_*` per deal action (by index in the negotiation's action array: 0 trade agreement, 3 military alliance, 6 Payments → `CAI_DEAL_COMPONENT_GIFT` with `-(amount)`, 7 peace, ...; the region action, index 4, has none) and scores the list with `0x101d7e26c`. The faction arguments are CAI wrappers; the wrapper's FACTION is at `+0x150` (found at runtime). The gift component (`0x30` bytes, ctor `0x1024f485c(self, aiContext, a, b, gold)`, vtable `0x104c1a2c0`) only has a deal value when the AI pays (`gold <= 0`, table lookup in vtable `+0x18`); received gold only gives a goodwill event (`+0x20`). So TWASE adds, for deals with regions, the negated score of "the AI pays V" (V = the region value from the Lua handler, + received / − given). Tested 2026-10-05 (WRE → ERE): demand 2,000 gold −3.1; Ajax (2,103) alone +3.4; demand 2,000 + Ajax −1.5; demand 2,000 + Aquae Sextiae (6,069) +13.1.

## Campaign UI controls (Steam arm64, 2026-10-28)

These mappings were independently derived from the supported Steam binary and its Binary Ninja view, without copying upstream source, scanners, or assembly implementations. The open Mac App Store database is a different build and was not used.

| Name | Mac VA / offset | Evidence |
|---|---|---|
| Encyclopedia base URL slot | `0x104ff0e48` | Real writable `const char*` in `__DATA`, initially pointing to `0x104892dde`, `"http://Atenc.totalwar.com/#"` |
| `ENCYCLOPEDIA_PAGES` constructor | `0x10287e7f0` | Reads Feral's `EncyclopaediaBaseURL` preference; may overwrite the slot with `NString_CStr(0x1054a82e0)` |
| Entry URL builder | `0x10287eaf8` | `(helper, const CA::String&) -> CA::String` through `x8`; slot load at `0x10287eb78`, then concatenates the configured language/base entry and requested path |
| Page URL builder | `0x10287ec24` | `(helper, int32 page) -> CA::String` through `x8`; slot load at `0x10287ecd4`, then concatenates the language/base and page paths |
| Settlement card initialize | `0x103457800` | `(CampaignSettlementCallback*, REGION*)`; stores region at `+0x68`, obtains actual building count at `+0x78`, visits `building_slot_1` through the bound at `+0x74`, initializing each existing child controller |
| Building-slot controller clear | `0x10345b30c` | `(slotController*)`; clears dynamic rows/helper and linked UI state. Native building refresh `0x10345d1c0` calls it before repopulating existing slots |
| Callback constructor/factory | `0x1034aac8c` | Allocates `0xa0` bytes, callback name `"CampaignSettlementCallback"`, vtable `0x104c99c08`; default flag false, bound 4 |
| Callback native reset | `0x10345a618` | Clears flag `+0x70`, restores bound 4 at `+0x74` |
| Callback capital presentation setup | `0x1034577ec` | Sets flag `+0x70` true and bound `+0x74` to 6 |
| Callback component / capital flag / bound | `+0x08` / `+0x70` / `+0x74` | `UIComponent*` / byte / `int32` |

The capital flag is **province-capital identity**, not simply the currently selected region. Panel population `0x103456db8` obtains the selected settlement's region with virtual `+0x38`, follows region `+0x4b0` to its province, and takes the capital region through the province's handle at `+0x80`. It compares that capital's region key (`+0x80`) against each province member's key. Only the match receives the `"capital"` template, flag true and bound 6 (inlined stores immediately before call `0x103457250`); the other members receive `"settlement"` and the default bound 4 (`0x103457320`). Thus TWASE's major/minor settings apply to province-capital/other cards respectively.

The URL hook preserves the latest native preference pointer across helper construction and restores it for a null/empty override. Custom storage is allocated/copied before publishing the real slot; its owner has process lifetime because hooks remain installed during macOS teardown. Lua setters and these native URL consumers run on the WinMain game thread. This is a prefix: native language and entry/page suffixes remain unchanged.

The slot hook temporarily changes the verified bound only during initialization, then restores the original field. With no override ever applied to that card class it directly forwards, without additional child lookups or mutation. Once used, an allocation-free per-class high-watermark tracks the managed range (at most 20 override slots). Reducing/clearing a setting clears and hides excess named children through the larger of this range and the original native bound; a native bound greater than 20 is not truncated. Cleanup uses the verified live per-slot clear function, not a hook on an unreferenced bulk-clear method. Additional lookups borrow stack `CA::String` descriptors: `UIComponent_FindChild` (`0x1032d4f88`) and its string comparator `0x101092e60` read only length/data and do not retain or destroy the descriptor. No hook-side heap allocation occurs per card call.

Before attaching, TWASE checks the 16-byte function prefixes:

- Constructor: `ff 43 02 d1 f4 4f 07 a9 fd 7b 08 a9 fd 03 02 91`.
- Initialize: `ff 83 03 d1 e9 23 07 6d fc 6f 08 a9 fa 67 09 a9`.
- Per-slot clear helper: `ff 03 01 d1 f6 57 01 a9 f4 4f 02 a9 fd 7b 03 a9`.
- Both URL loads: `88 3b 01 d0 00 25 47 f9` (`adrp x8, 0x104ff0000; ldr x0, [x8, #0xe48]`); also checks the native literal including its terminator.

For a non-browser native smoke check, the settlement info button (`0x1034585a4`, call at `0x10345862c`) uses page 13 with helper `*(*g_Campaign + 0x228)`. Call the page builder with a non-trivial result wrapper containing `TempString` to use the arm64 `x8` result ABI, inspect the result, then call `String_dtor`. Existing UI rows can be inspected below `settlement_panel/main_settlement_panel`: child count/data at `UIComponent +0x114/+0x118`, controller at `+0x258`, visibility byte at `+0x199`. Select another region and back to rebuild the panel. Address/layout evidence is static; this mapping does not claim a build or live smoke test.

For an existing-card smoke check on the game thread, obtain the UI root from `0x105604ea0`, find the panel above, and verify each row's callback vtable equals `0x104c99c08` before reading its saved region at `+0x68`. To reinitialize a row in place, first clear each existing slot controller with `0x10345b30c`, following the native refresh's clear-before-repopulate sequence, then call hooked `0x103457800(callback, savedRegion)`. Compare visibility with override 2 and after resetting to the native bound, which is restored in the callback field after every call. If no real panel/card exists, report that prerequisite rather than fabricating one.

### Runtime verification checkpoint (2026-10-28)

Built and installed TWASE `1d01724` (including the feature code from `f14d33b`) against the supported Steam arm64
game. The native constructor and settlement hooks attached successfully. The updated `campaign_ui_dev` harness
passed all 35 in-game API checks and restored the prior values: native URL `http://Atenc.totalwar.com/#`, no major
or minor override. Its standalone Lua regressions also passed after the Windows test-kit fixes.

The test campaign loaded successfully and exited with status 0 after a quit request. This run stopped before
navigation/rendering acceptance checks because screen-capture permission was unavailable to the running session;
the user enabled the permission and needs to restart the session before further visual testing.

No building/unit/technology link navigation, Feral-preference restoration, rendered slot transitions, hidden-building
persistence, horde/desolate behavior, save-load persistence, or DLC campaign result is claimed by this checkpoint.
Those remain the next acceptance checks. The debug harness was removed and the loopback probe stopped.
Runtime evidence: local TWASE log `twase-2026-10-28-11-43-50.log`.

### Manual acceptance and feature closure (2026-10-28)

After restarting, the user reported successful URL override/reset and the slot-limit cases they exercised.
The user then confirmed expanded settlement slots work in The Dawnless Days (TDD), testing the requested
`campaign_ui_dev.slots(12, 9)` override. These are user-observed results, not an automated exhaustive acceptance run.

The updated `campaign_ui_dev` now includes TDD's `campaigns/main_attila_map` entrypoint. The deployed
`tdd_twase_slots.pack` replaces TDD's Windows-only patcher with TWASE's slot and encyclopedia APIs. The user
confirmed that slots automatically expand to 12/9 and TDD's encyclopedia URL works, without console setup.

The user also reported verifying the remaining edge cases and considers acceptance complete. These are manual
user-reported results; no additional automated trace or exhaustive per-slot record was collected. Charlemagne
and The Last Roman panels are explicitly deferred by the user, not marked passed. No blocking production
defect remains known from this acceptance work.

The disposable harness was redeployed for manual testing; remove it afterward with
`tools/deploy.sh --remove campaign_ui_dev` from the attila-mods checkout.

## App Store arm64 RC4

Build: Feral 1.6.1 RC4, `CFBundleVersion 480969.104294`, executable UUID
`196C74A0-71A2-3C28-80DF-BC3DB1C7FEB7`. Original executable SHA-256:
`184aa3ce055f50e3a51318bbc242d19106aaa07853daa27632417da81111b363`.
The original Steam executable used for comparison has SHA-256
`13f5d523019f291f489353fa5d3661bc9a668bb2b0375d6c3201e01d74525c5e`.

The 111 executable/global address pairs are recorded directly as `{Steam, App Store}` in
`src/platform/macos/sdk/Attila/Addresses.hpp` and `Lua/Addresses.hpp`; offsets within objects remain integers.
Each target was matched independently, not by a global delta. Full containing routines were compared after
normalizing PC-relative references and proven ADRP-derived global page offsets, preserving ordinary field offsets
and register clobbers. Duplicate routines were disambiguated with callers, surrounding context or named tables.
The diplomacy dropdown vtable was checked through constructor references and 48 function-pointer slots.
The changed obfuscated write-probe routine was checked separately through both disassemblies, its anchor import,
permission helpers and failure tail. Analysis databases were not modified.

| Integration-sensitive target | App Store VA | Expected instruction(s) |
| --- | --- | --- |
| Clean-text anchor | `0x10025ca1c` | weak import `COpenGLProgramAspectManager::InitialiseThreadLocals` |
| Anchor slot | `0x104a7f5a0` | runtime pointer to the anchor, not its raw chained-fixup encoding |
| Startup anchor call | `0x100f6459c` | `0x94c78524`, BL to weak stub `0x104145a2c`; replace with direct BL `0x97cbe120` |
| Write probe | `0x10025cac8` | `0xa9be4ff4`; live copy gets `ret`, immutable snapshot retains the original |
| Lua sink store | `0x103f24734` | `0xf9047d00`, `str x0, [x8, #0x8f8]` |
| Encyclopedia entry/page loads | `0x10287d4c0` / `0x10287d61c` | `{0x90013ba8, 0xf940bd00}`, loading slot `0x104ff1178` |
| Native encyclopedia string | `0x10489162e` | `http://Atenc.totalwar.com/#`, including terminator |
| Unit-size patch | `0x101a505a0` | `0x54002640` to `0x54000080`, same verified branch shapes as Steam |

### App Store runtime verification (2026-10-28)

An APFS-cloned, ad-hoc-signed, unsandboxed copy loaded a probe and reached the native frontend without patching
receipt/licence validation. The actual `twase-appstore.command --prepare` flow then prepared another copy,
preserved its receipt, and passed deep/strict signature verification. The installed `/Applications` app was not
modified. Both `HOME` and `CFFIXED_USER_HOME` routed actual Feral writes to the private development home.

The release-mode TWASE build initialized the RC4 clean-text anchor, unit-size patch, Lua log, game tick, startup,
diplomacy, campaign UI, native-region-trading and Metal hooks. The Steam Workshop ordering hook is intentionally
not installed on this storefront. The frontend and in-game console rendered; clean frontend exit returned 0.

`--load "Saxons 395 AD Spring.save"` loaded a copied vanilla save in the private home. Two local packs loaded
through Feral's VFS; all 35 campaign UI API checks passed and restored prior settings. A throwaway real-campaign
smoke reported Tulifurdum / `att_prov_frisia`, wealth 514, ERE-to-WRE attitude 27, and three building DB records,
also exercising region localisation. The campaign rendered windowed (`gfx_fullscreen false`), preserving the
user's display preference.

On the real Tulifurdum card, `slots(2, 2)` rendered two building widgets; `inspect()` reported `2/6 [1,2]`.
After resetting and reopening, the original building icons returned and `inspect()` reported
`6/6 [1,2,3,4,5,6]`. The other two province members were not player-owned and exposed no building widgets; this
does not establish minor-card coverage. API-level URL override/reset passed, but a building right-click only
opened the native building browser; no external navigation pass is claimed. Expanded/TDD/DLC layouts,
end-turn construction, battles and native diplomatic transfers were not exercised by this smoke.

Runtime evidence: local logs `twase-2026-10-28-22-33-26.log` and `twase-2026-10-28-22-40-54.log`.

The App Store campaign exited with status 0. The launcher restored `GameOptionsDialogShouldShow=1`,
`ExtraCommandLineEnabled=0`, and an empty `ExtraCommandLine`, while retaining the user's `FullScreen=0`.
Negative smoke checks refused injection into the installed original and preparation over an existing destination.
The original executable SHA-256 and Feral signing team/sandbox entitlements remained unchanged.
Disposable smoke packs/probes were removed after verification.

Steam compatibility was exercised with the same new dylib and a separate temporary Feral home, without replacing
the installed Steam dylib: the Steam UUID selected its original targets, clean-text anchor and all hooks attached,
the vanilla campaign loaded, all 35 campaign UI API checks passed, and the process exited with status 0.
Evidence: Steam local log `twase-2026-10-28-22-46-45.log`. This was a build-selection/API regression smoke, not a
repeat of the earlier full Steam acceptance run.


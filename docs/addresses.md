# Address table

Build: Feral 1.6.1 RC2, `CFBundleVersion 480285.103778`, `LC_UUID 392D6F66-9183-329E-8A98-8C90FC828326`.

Addresses are **unslid VAs** as shown in Binary Ninja (image base `0x100000000`); runtime address = VA + ASLR slide.
All of them are also named in the bndb.

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

## Lua API for mods (`twase.*`, `src/dylib/Scripting/LuaApi.cpp`)

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

## Database (loaded DB tables, game + mods merged)

Reached from the campaign global the UI uses: world = `*(*(*0x105608ea0 + 0x30) + 0x48)`, DB container = `DB_GetContainer(world + 0x21e0)` (`0x101e76780`: `*(*(*field + 0x58) + 0x20)`). Each table has a lazy accessor `DATABASE_TABLE* (container)` that loads it on first use (`"Loading database: %s"`, the name string `<table>_table` is referenced only by its accessor) and caches it at a fixed container offset. Lookup by key: `StringHashMap_Find(table + 0x18, const TempString* key)` (`0x1010e5040`) → `int* index`, records at `table + 0x10` (`RECORD**`), count at `table + 0x0c`. Records start with their key (`TempString` at `+0`) and follow the schema's column order; references to other tables are record pointers. Junction tables have composite keys, so they're scanned. Layouts found by dumping records in game (`twase.dev_db_record`) and matching the extracted TSVs.

| Table | Accessor | Container offset | Fields |
|---|---|---|---|
| building_levels | `0x102c86dfc` | `0x1fb0` | `+0x10` chain record*, `+0x18` level, `+0x1c` create_time, `+0x20` create_cost, `+0x24` upkeep_cost (int32) |
| main_units | `0x102c90940` | `0x14d8` | `+0x30` create_time, `+0x40` num_men, `+0x48` multiplayer_cost, `+0x68` recruitment_cost, `+0x88` upkeep_cost, `+0xb8` unique_index |
| region_to_province_junctions | `0x102cc5c08` | `0x1630` | 0x20-byte records: `+0x00` province key, `+0x10` region key (TempStrings) |
| provinces | `0x102c8ea88` | `0x1638` | key at `+0`, record built by `0x10298a390` (key + loc entry at `+0x10`) |

`REGION + 0x78` = its `regions` record (the Lua `name()`), `REGION + 0x4b0` → the campaign province object (`**`), whose per-faction parts are at `+0x94` (count) / `+0x98` (`sub_102410864`).

Tested 2026-10-04 (unattended runs with Fireforged): `twase.building_level`, `twase.unit` and `twase.region_province` return the TSV / UI values (e.g. Olisipo → `att_prov_lusitania`).

## Localisation

The loc manager is at `DB container + 0x2200` (`sub_1030f0534`). Its virtual `+0x30` is the lookup: `const WString* (loc, const TempString* key, int)` (callers pass 1), returning the UTF-16 text (`CA::UniString`) or an empty / null string for a missing key. Used e.g. by `0x10298a390` (`provinces_onscreen_<key>`) and `0x101cb17c0` (the GDP category names). Tested 2026-10-04: `twase.localise` returns region, province, faction and UI strings; missing keys give nil.

## Native region trading (Rome II leftover)

Attila keeps the whole region trading feature: the diplomatic action type 6 (`EMPIRECAMPAIGNAI::DIPLOMATIC_ACTION_TRADE_REGIONS`, vtable `0x104c1e520`, its slot 13 `0x1025c8188` executes the deal and transfers the regions), the permission "regions" (`force_diplomacy` id 4; action type → permission table at `0x10466c5a0`, 2-bit permissions at `relationship + 0x90`, setter `0x101cf7a40`, getter `0x101cf79e4`), the game command `CCQ_DIPLOMACY_PROPOSE_TRADE_REGIONS` and the UI (`diplomacy_hud`: `offer_button_trade_regions` / `DiplomacyListTradeRegionsButton`, `trade_regions` / `DiplomacyTradeRegionsPanel`, loc "Gift Region").

The negotiation creates the action but never marks it available (`+0x18`), so the "Add Offer/Demand" list (built by `0x1036cca88(context, offers, demands)` from the available offer-side actions, context = `DiplomacyDropdown + 0x200`, negotiation = context `+ 0xd0`) never shows it. The actions are at negotiation `+0x710` (count `+0x70c`), only the campaign's types, not indexed by type. Action layout: `+0x8` negotiation, `+0x10` side (1 offer), `+0x14` type, `+0x18` available, `+0x19` blocked; the region action keeps its offered / demanded regions at `+0x20/+0x24/+0x28` and `+0x30/+0x34/+0x38`.

`Hooks/NativeRegionTrading.cpp` marks the region action available (unless blocked) right before the list is built. Tested 2026-10-04: "Gift Region" appears first in the list and opens the native Gift Region panel; its region list was empty (WRE → ERE), to investigate.

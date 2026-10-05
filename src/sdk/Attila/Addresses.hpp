#pragma once

#include <cstdint>

// Unslid addresses as shown in Binary Ninja (image base 0x100000000), resolve with Image::Resolve.
// See docs/addresses.md for how each one was found.
namespace sdk::Attila::Addresses
{
// Feral 1.6.1 RC2 (CFBundleVersion 480285.103778)
constexpr const char* SupportedUUID = "392D6F66-9183-329E-8A98-8C90FC828326";

// STARTUP
// int64_t (*)(void* instance, void* prevInstance, const char16_t* commandLine), CA's WinMain on the WinMain thread
constexpr uint64_t WinMain = 0x1032BC10C;

// GAME LOOP
// bool (*)(void* app, int, int, int, int), called once per frame by the run loop on the WinMain (game/Lua) thread
constexpr uint64_t GameTick = 0x1010ECD24;

// LUA LOG
// void (*)(const char*), called by print, out and the Lua error handler, falls back to fputs(stdout) if null
constexpr uint64_t g_LuaLogSink = 0x1056D6588;

// void (*)(void (*sink)(const char*)), the str instruction that stores the sink
constexpr uint64_t SetLuaLogSink_Store = 0x103F25DEC;

// SCRIPT RUNTIMES
// Linked list of all active lua states
constexpr uint64_t g_RuntimeLuaListHead = 0x10560B7C8;
constexpr uint64_t g_RuntimeLuaListSentinel = 0x10560B7D0;

// lua_State* (*)(void* scriptInterface)
constexpr uint64_t GetLuaState = 0x1038B377C;

// MOD LOADER
// void* (*)(ScriptingEnv* self), EPISODIC_SCRIPTING_ENV virtual slot 3, loads and runs campaigns/<name>/scripting.lua
constexpr uint64_t RunStartupPath = 0x102170104;

// VFS
// void* (*)(), returns the static VFS instance
constexpr uint64_t VFS_GetInstance = 0x10387C0A8;

// void (*)(void* vfs, CName* dir, CName* pattern, VFSSearchResults* out, int flags, int mode), a virtual of the VFS
constexpr uint64_t VFS_SearchFiles_VtableOffset = 0x58;

// CName* (*)(CName* self, const char* str)
constexpr uint64_t CName_ctor = 0x10109240C;

// void (*)(void* ptr), the game allocator's free
constexpr uint64_t tw_free = 0x1010ACB70;

// DIPLOMACY
// float (*)(void* deal, void* ctx, void* factionA, void* factionB)
// UI-only wrapper around the deal evaluator (0x101d7b25c), AI accepts deals with score >= 0
constexpr uint64_t Diplo_GetDisplayedDealScore = 0x101D7E890;

// int (*)(void* caiModule, void* deal)
// buckets the score: 2 low, 3 moderate, 4 high, thresholds are the tweakers
// CAI_DIPLOMACY_NEGOTIATION_DISPLAYED_LIKELIHOOD_THRESHOLD_LOW/HIGH (-4 / 4, value at +0x64 of 0x105600378 / 0x1056003e0)
constexpr uint64_t CAI_GetDealLikelihoodBucket = 0x102296A5C;

// void* (*)(DiplomacyDropdown* self, int likelihood (-1/0/1, -2 hidden), bool show)
constexpr uint64_t DiplomacyDropdown_SetLikelihood = 0x1035F4A1C;

// UI
// void (*)(UIComponent* self, const WString* text, bool allStates)
constexpr uint64_t UIComponent_SetTooltipText = 0x1032D2418;

// STRINGS
// TempString* (*)(TempString* self, const char* utf8)
constexpr uint64_t String_ctor = 0x10104CE1C;

// void (*)(TempString* self)
constexpr uint64_t String_dtor = 0x10104BA58;

// WStringResult (*)(const TempString* src), returns through x8
constexpr uint64_t String_ToUniString = 0x101049BB0;

// void (*)(WString* self)
constexpr uint64_t UniString_dtor = 0x101049D20;

// CAMPAIGN MODEL (for the twase.* Lua API)
// bool (*)(lua_State* L, int idx, FACTION_SCRIPT_INTERFACE** out), the game's own faction argument check:
// 0 if the value isn't a userdata, a Lua type error if it's another userdata type
constexpr uint64_t FactionScriptInterface_GetArg = 0x101D5C640;

// int (*)(CAI_INTERFACE* self, const FACTION& a, const FACTION& b), EMPIRECAMPAIGNAI::CAI_INTERFACE::get_faction_attitude:
// sums the attitude components, rounded. Found via the RTTI of its lambda (vtable 0x104c12268)
constexpr uint64_t CAI_GetFactionAttitude = 0x10229D814;

// Offsets
constexpr uint64_t FactionScriptInterface_Faction = 0x10;  // FACTION*
constexpr uint64_t Faction_ModelHandle = 0x18;             // -> +0x48 the campaign world
constexpr uint64_t ModelHandle_World = 0x48;
constexpr uint64_t World_CAIInterface = 0x2D60;            // CAI_INTERFACE*
constexpr uint64_t Faction_AliveMarker = 0x8F8;            // null once the faction is destroyed (FACTION::is_dead 0x101b65694)
constexpr uint64_t RegionScriptInterface_Region = 0x10;    // REGION*
constexpr uint64_t Region_Wealth = 0x16C;                  // int, the region's wealth (GDP) as the province panel shows it
constexpr uint64_t Region_WealthBreakdown = 0x7A0;         // RegionWealthCategory[15], the wealth breakdown tooltip's data
constexpr int RegionWealthCategoryCount = 15;

// DATABASE (the loaded DB tables, game + mods merged)
// The campaign global the UI uses to reach the world: *(*(*g_Campaign + 0x30) + 0x48)
constexpr uint64_t g_Campaign = 0x105608EA0;
constexpr uint64_t Campaign_Handle = 0x30;
constexpr uint64_t World_Database = 0x21E0;                // DB_GetContainer(world + 0x21e0)
// void* (*)(void** worldDatabaseField) = *(*(*field + 0x58) + 0x20), the DB container
constexpr uint64_t DB_GetContainer = 0x101E76780;
// DB table accessors: DATABASE_TABLE* (*)(void* container), load the table on first use ("Loading database: %s")
constexpr uint64_t DB_BuildingLevels = 0x102C86DFC;        // container + 0x1fb0
constexpr uint64_t DB_RegionToProvinceJunctions = 0x102CC5C08; // container + 0x1630
constexpr uint64_t DB_Provinces = 0x102C8EA88;             // container + 0x1638
constexpr uint64_t DB_MainUnits = 0x102C90940;             // container + 0x14d8
// int* (*)(StringHashMap* map, const TempString* key), the record index or null
constexpr uint64_t StringHashMap_Find = 0x1010E5040;

// LOCALISATION
constexpr uint64_t Database_Localisation = 0x2200;         // container + 0x2200 (sub_1030f0534)
// virtual +0x30: const WString* (*)(void* loc, const TempString* key, int), the game's loc lookup
constexpr uint64_t Localisation_LookupVtableOffset = 0x30;

// DATABASE_TABLE
constexpr uint64_t DBTable_Count = 0x0C;                   // uint32
constexpr uint64_t DBTable_Records = 0x10;                 // RECORD**
constexpr uint64_t DBTable_Index = 0x18;                   // StringHashMap key -> record index

// Records start with their key (TempString at +0). Offsets found by dumping records and matching the TSVs (Fireforged data)
constexpr uint64_t Region_Record = 0x78;                   // REGION -> regions record (the Lua name() reads its key)
constexpr uint64_t BuildingLevel_Chain = 0x10;             // building_chains record*
constexpr uint64_t BuildingLevel_Level = 0x18;             // int32
constexpr uint64_t BuildingLevel_CreateTime = 0x1C;        // int32
constexpr uint64_t BuildingLevel_CreateCost = 0x20;        // int32
constexpr uint64_t BuildingLevel_UpkeepCost = 0x24;        // int32
constexpr uint64_t MainUnit_CreateTime = 0x30;             // int32
constexpr uint64_t MainUnit_NumMen = 0x40;                 // int32
constexpr uint64_t MainUnit_RecruitmentCost = 0x68;        // int32 (multiplayer_cost at 0x48)
constexpr uint64_t MainUnit_UpkeepCost = 0x88;             // int32
constexpr uint64_t RegionToProvince_Province = 0x00;       // TempString, records are 0x20 apart
constexpr uint64_t RegionToProvince_Region = 0x10;         // TempString

// NATIVE REGION TRADING ("Gift Region", see Hooks/NativeRegionTrading.cpp)
// RegionList (*)(FACTION* from, FACTION* to) through x8: from's regions that can be given, not the capital, no garrisoned
// army / navy; with `to`, only regions where an army of `to` stands (the horde settling rule). Used by the region panel
// (0x1035adce8: offers from -> to, demands to -> from) and by the availability rule below
constexpr uint64_t Faction_GiftableRegions = 0x101BDAD20;
// bool (*)(FACTION* a, FACTION* b), is the trade regions action (type 6) available: one side human, the other a horde,
// and giftable regions exist (called by the action's availability update 0x1025c79a0)
constexpr uint64_t DiplomaticAction_RegionTradePossible = 0x1025C7A6C;
constexpr uint64_t Faction_IsHuman = 0x970;                // bool (the Lua is_human)
constexpr uint64_t Faction_IsHorde = 0x101B521EC;          // void* (*)(FACTION*), non-null for hordes (the Lua is_horde)

// The native region panel (UIDLL::DiplomacyTradeRegionsPanel, layout diplomacy_hud "trade_regions")
// void (*)(PANEL* self, DIPLOMATIC_ACTION* action): fills list_offers (player -> AI) and list_demands (AI -> player)
constexpr uint64_t TradeRegionsPanel_Populate = 0x1035AD948;
constexpr uint64_t TradeRegionsPanel_Component = 0x8;      // UIComponent*
// UIComponent* (*)(UIComponent* parent, const TempString* name, bool recursive)
constexpr uint64_t UIComponent_FindChild = 0x1032D4F88;
// void (*)(UIComponent*, bool), the engine side of the Lua SetVisible
constexpr uint64_t UIComponent_SetVisible = 0x1032C6B00;

// Region value in the AI's deal evaluation
// float (*)(NEGOTIATION* deal, void* aiContext, FACTION* a, FACTION* b, int32_t* out, bool* out)
constexpr uint64_t Diplo_EvaluateDeal = 0x101D7B25C;
// float (*)(ComponentList* components, void* aiContext, FACTION* a, ...): scores the deal's CAI_DEAL_COMPONENT_* list
// (entries {component*, int32 index}, 16 bytes); called once at the end of Diplo_EvaluateDeal
constexpr uint64_t Diplo_ScoreComponents = 0x101D7E26C;
// void (*)(CAI_DEAL_COMPONENT_GIFT* self, void* aiContext, FACTION* a, FACTION* b, int32_t gold), 0x30 bytes;
// the evaluator builds it for Payments with gold = -(payment amount)
constexpr uint64_t DealComponentGift_ctor = 0x1024F485C;
constexpr uint64_t DealComponentGift_Size = 0x30;
constexpr uint64_t DiplomaticAction_InDealVtableOffset = 0x50; // bool (*)(action): part of the deal
// trade regions action: regions in list 1 go to negotiation + 0x20, in list 2 to negotiation + 0x28 (its execute 0x1025c8188)
constexpr uint64_t TradeRegions_List1Count = 0x24;         // REGION** at + 0x28
constexpr uint64_t TradeRegions_List2Count = 0x34;         // REGION** at + 0x38
constexpr uint64_t Negotiation_FactionA = 0x20;            // FACTION*
constexpr uint64_t Negotiation_FactionB = 0x28;            // FACTION*
constexpr uint64_t Faction_Record = 0x8F8;                 // factions record (key TempString at + 0), null once destroyed

// Diplomacy internals used by the dev probes
constexpr uint64_t DiplomacyContext_Negotiation = 0xD0;    // DiplomacyDropdown + 0x200 = context
constexpr uint64_t Negotiation_ActionCount = 0x70C;        // uint32
constexpr uint64_t Negotiation_Actions = 0x710;            // DIPLOMATIC_ACTION**, only the campaign's types (not indexed by type)
constexpr int DiplomaticAction_TradeRegions = 6;           // permission 4 "regions" (type -> permission table 0x10466c5a0)
constexpr uint64_t DiplomaticAction_Type = 0x14;           // int32; + 0x10 side (1 offer), + 0x18 available, + 0x19 blocked

// PATCHES
constexpr uint64_t BitSetCrashAddr = 0x101A51C58;

} // namespace sdk::Attila::Addresses

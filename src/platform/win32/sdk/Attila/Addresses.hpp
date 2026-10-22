#pragma once

namespace sdk::Attila::Addresses
{
	// hooks
	constexpr uint32_t LuaLog = 0x01A76CF0;
	constexpr uint32_t RunStartupPath = 0x79B980;

	// patches
	constexpr uint32_t BitSetCrashAddr = 0x0091CB57;


	// VFS
	constexpr uint32_t VFS_GetInstance = 0x1658C80;
	constexpr uint32_t VFS_SearchFiles = 0x1635B50;

	// Names
	constexpr uint32_t CName_ctor = 0xDF290;

	// base
	constexpr uint32_t tw_free = 0xE98C0;


	// LUA
	// Direct ScriptInterface* pointers
	constexpr uint32_t g_BattleScriptInterface  = 0x1F49E0C;
	constexpr uint32_t g_FrontendScriptInterface = 0x287CFB8;

	// Points to this+0x18, use getter convention like the game does
	constexpr uint32_t g_CampaignScriptInterface = 0x287CFD4;

	// Shared — holds last created LoggedScriptInterface
	constexpr uint32_t g_ConditionsEffects = 0x1CCE44C; // TODO: stale, not yet re-located for the current game version

	// Linked list of all active lua states
	constexpr uint32_t g_RuntimeLuaListHead     = 0x01DA4230;
	constexpr uint32_t g_RuntimeLuaListSentinel = 0x01DA4234;

	// Master lua state
	constexpr uint32_t g_RuntimeLuaState = 0x0291DE00;

	// lua_State* (__thiscall*)(void* thisPtr)
	constexpr uint32_t GetLuaState = 0x1626AA0;


	// DIPLOMACY
	// float (__cdecl*)(void* deal, int ctx, void* factionA, void* factionB)
	// UI-only wrapper around the deal evaluator, AI accepts deals with score >= 0
	constexpr uint32_t Diplo_GetDisplayedDealScore = 0xA6E980;

	// int (__thiscall*)(void* caiModule, void* deal, int)
	// buckets the score: <= LOW tweaker (-4) low, >= HIGH tweaker (4) high, else moderate
	constexpr uint32_t CAI_GetDealLikelihoodBucket = 0xD2A7A0;

	// int (__thiscall*)(DiplomacyDropdown* this, int likelihood (-1/0/1, -2 hidden), bool show)
	constexpr uint32_t DiplomacyDropdown_SetLikelihood = 0x14F95A0;


	// UI
	// void (__thiscall*)(UIComponent* this, const WString* text, bool allStates)
	constexpr uint32_t UIComponent_SetText = 0x13B8B40;

	// void (__thiscall*)(UIComponent* this, const WString* text, bool allStates)
	constexpr uint32_t UIComponent_SetTooltipText = 0x13B9A00;

	// WString* (__thiscall*)(WString* this, const wchar_t* str)
	constexpr uint32_t WString_ctor = 0xDFEF0;

	// void (__thiscall*)(WString* this)
	constexpr uint32_t WString_dtor = 0xE0720;


	// FACTIONS (twase.faction_attitude)
	// bool (__cdecl*)(lua_State* L, int idx, FACTION_SCRIPT_INTERFACE** out): the type check of the faction methods'
	// arguments, luaL_typerror for other userdata; called by the single argument check used by allied_with etc.
	constexpr uint32_t FactionScriptInterface_GetArg = 0x80C480;

	// float (__thiscall*)(CAI_INTERFACE* self, FACTION* a, FACTION* b), a's attitude towards b. Found through the attitude
	// icon of the diplomacy screens (UI_SetAttitudeIcon 0x1608AE0), which truncates it to int like Mac's rounded int
	constexpr uint32_t CAI_GetFactionAttitude = 0xD38000;
	using FactionAttitudeValue = float;

	// Offsets
	constexpr uint32_t FactionScriptInterface_Faction = 0x8;   // FACTION*
	constexpr uint32_t Faction_ModelHandle = 0x158;            // -> + 0x28 the campaign world (ModelHandle_GetWorld 0x9BCB40)
	constexpr uint32_t ModelHandle_World = 0x28;
	constexpr uint32_t World_CAIInterface = 0x1C44;            // CAI_INTERFACE*
	constexpr uint32_t Faction_AliveMarker = 0x800;            // null once the faction is destroyed (FACTION::is_dead 0x716690)


	// REGIONS (twase.region_wealth), from REGION_SCRIPT_INTERFACE's methods (table 0x1D2D7B8) and the province panel
	// (ProvincePanel_Update 0x145E540)
	constexpr uint32_t RegionScriptInterface_Region = 0x8;     // REGION*
	constexpr uint32_t Region_Record = 0x50;                   // regions record (key TempString at + 0), the Lua name()
	constexpr uint32_t Region_Wealth = 0xF4;                   // int, the province panel's "wealth" (region_wealth_change_percent + 0xFC, town_wealth_growth + 0x10C)
	constexpr uint32_t Region_WealthBreakdown = 0x570;         // RegionWealthCategory[15], the wealth_breakdown_tooltip's data (CampaignTooltip_SetWealthBreakdown 0x1457F90)
	constexpr int RegionWealthCategoryCount = 15;              // loc keys campaign_localised_strings_string_gdp_* at 0x1D2C64C, the Mac order


	// DATABASE (the loaded DB tables), world = *(*(*g_Campaign + 0xA54) + 0x28) like the UI (UI_SetAttitudeIcon)
	constexpr uint32_t g_Campaign = 0x28D52B0;
	constexpr uint32_t Campaign_Handle = 0xA54;
	constexpr uint32_t World_Database = 0x10F0;                // DB_GetContainer(world + 0x10f0)

	// void* (__thiscall*)(void* worldDatabaseField): *(*(*field + 0x30) + 0x10)
	constexpr uint32_t DB_GetContainer = 0x9D09F0;

	// DATABASE_TABLE* (__thiscall*)(void* container), lazy, the only code referencing "<table>_table"
	constexpr uint32_t DB_BuildingLevels = 0xE1B690;
	constexpr uint32_t DB_RegionToProvinceJunctions = 0xE39470;
	constexpr uint32_t DB_Provinces = 0xE389E0;
	constexpr uint32_t DB_MainUnits = 0xE33D00;
	constexpr uint32_t DB_FactionToMercenarySetJunctions = 0xE30210;
	constexpr uint32_t DB_MercenaryPoolToGroupsJunctions = 0xE34170;
	constexpr uint32_t DB_MercenaryUnitGroups = 0xE34520;

	// int* (__thiscall*)(StringHashMap* map, const TempString* key), the record index or null (DBTable_GetRecordByKey 0x19AA20)
	constexpr uint32_t StringHashMap_Find = 0xCF7C0;

	// TempString* (__thiscall*)(TempString* self, const char* utf8) / void (__thiscall*)(TempString* self)
	constexpr uint32_t String_ctor = 0xDFCD0;
	constexpr uint32_t String_dtor = 0xE06F0;

	// DATABASE_TABLE
	constexpr uint32_t DBTable_Count = 0x08;                   // uint32
	constexpr uint32_t DBTable_Records = 0x0C;                 // RECORD**
	constexpr uint32_t DBTable_Index = 0x10;                   // StringHashMap key -> record index

	// RECORDS: keys are TempStrings at + 0, references to other tables are record pointers. Checked 2026-10-22 with
	// twase.dev_db_fields against the TSV export (values of the first record; create_cost differs, a mod changes it)
	constexpr uint32_t BuildingLevel_Chain = 0x0C;             // building_chains record* (BuildingLevelRecord_ctor 0xF02E40)
	constexpr uint32_t BuildingLevel_Level = 0x10;             // int32
	constexpr uint32_t BuildingLevel_CreateTime = 0x14;        // int32
	constexpr uint32_t BuildingLevel_CreateCost = 0x18;        // int32
	constexpr uint32_t BuildingLevel_UpkeepCost = 0x1C;        // int32
	constexpr uint32_t MainUnit_CreateTime = 0x1C;             // int32 (is_naval at 0x20, land_unit* 0x24), 0xD8-byte records
	constexpr uint32_t MainUnit_NumMen = 0x28;                 // int32
	constexpr uint32_t MainUnit_RecruitmentCost = 0x48;        // int32 (multiplayer_cost at 0x30)
	constexpr uint32_t MainUnit_UpkeepCost = 0x5C;             // int32
	constexpr uint32_t RegionToProvince_Province = 0x00;       // TempString, records are 0x18 apart
	constexpr uint32_t RegionToProvince_Region = 0x0C;         // TempString
	// levy pools (twase.levy_pool)
	constexpr uint32_t MercenarySet_Faction = 0x00;            // faction_to_mercenary_set_junctions: factions record*, 8-byte rows
	constexpr uint32_t MercenarySet_Pool = 0x04;               // mercenary pool record*
	constexpr uint32_t PoolGroup_Pool = 0x04;                  // mercenary_pool_to_groups_junctions: mercenary pool record*, 0x1C-byte rows (id at 0)
	constexpr uint32_t PoolGroup_Group = 0x08;                 // mercenary_unit_groups record*
	constexpr uint32_t PoolGroup_InitialCount = 0x0C;          // int32 initial_unit_count
	constexpr uint32_t MercenaryGroup_ChanceToReplenish = 0x00; // mercenary_unit_groups: float, 0x1C-byte records
	constexpr uint32_t MercenaryGroup_Key = 0x04;              // TempString
	constexpr uint32_t MercenaryGroup_MaxCount = 0x10;         // int32
	constexpr uint32_t MercenaryGroup_MaxReplenishPerTurn = 0x14; // int32
	constexpr uint32_t MercenaryGroup_Unit = 0x18;             // main_units record*

} // namespace sdk::Attila::Addresses

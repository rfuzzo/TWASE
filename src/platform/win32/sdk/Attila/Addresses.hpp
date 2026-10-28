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

	// CAMPAIGN UI: verified against the x86 2023-06-18 image, not fixed
	// function RVAs (that image predates the table above). CampaignUI.cpp
	// resolves the loaded PE and checks the slot loop/encyclopedia consumers.
	// The flag identifies the province-capital presentation card.
	constexpr uint32_t SettlementCallback_Root = 0x4;        // UIComponent*, Initialize's lookup parent
	constexpr uint32_t SettlementCallback_MainCard = 0x44;   // byte, SetAsCapital writes 1
	constexpr uint32_t SettlementCallback_MaxSlots = 0x48;   // uint32_t, Construct 4 / SetAsCapital 6

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

	// LOCALISATION (twase.localise): the loc manager at container + 0x1694 (DB_GetLocalisation 0xE0D270); its virtual
	// + 0x0 is the lookup const WString* (loc, const TempString* key, int), callers pass 1 (ProvinceRecord_SetOnscreenName
	// 0x10737A0 with "provinces_onscreen_<key>")
	constexpr uint32_t Database_Localisation = 0x1694;
	constexpr uint32_t Localisation_LookupVtableOffset = 0x0;

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


	// NATIVE REGION TRADING ("Gift Region", src/core/Hooks/NativeRegionTrading.cpp), the Mac names
	// RegionList* (__thiscall*)(FACTION* from, RegionList* out, FACTION* to), retn 8: from's regions that can be given
	// (with `to`, only where an army of `to` stands); called by the availability rule and the panel's fill 0x1603DF0
	constexpr uint32_t Faction_GiftableRegions = 0x736B40;
	// bool (__cdecl*)(FACTION* a, FACTION* b): one side human, the other a horde that can own land, at peace, giftable
	// regions; only the trade regions action's ctor 0xBFCC00 calls it, with the negotiation's factions
	constexpr uint32_t DiplomaticAction_RegionTradePossible = 0xC1ABB0;
	constexpr uint32_t Faction_IsHuman = 0x84C;                // bool (the Lua is_human 0x84B970)
	constexpr uint32_t Faction_RegionCount = 0x8D4;            // u32, REGION** at + 0x8d8 (the Lua region_list 0x85CC80)
	constexpr uint32_t Faction_CanOwnLand = 0x12A1;            // bool (FACTION::can_own_land 0x715BA0)
	// bool (__thiscall*)(FACTION*), FACTION::is_horde (Mac's returns a pointer)
	constexpr uint32_t Faction_IsHorde = 0x716320;
	using FactionIsHordeValue = bool;
	constexpr uint32_t Faction_Record = 0x800;                 // factions record (key TempString at + 0), null once destroyed (FACTION::record 0x72B890)

	// DIPLOMATIC_ACTION (ctor 0xBFB9F0): negotiation, side (1 offer), type, available, blocked
	constexpr uint32_t DiplomaticAction_Negotiation = 0x4;
	constexpr uint32_t DiplomaticAction_Side = 0x8;            // int32
	constexpr uint32_t DiplomaticAction_Type = 0xC;            // int32
	constexpr uint32_t DiplomaticAction_Available = 0x10;      // bool, + 0x11 blocked
	constexpr uint32_t DiplomaticAction_Flags = 0x10;          // uint8 available, uint8 blocked
	constexpr uint32_t DiplomaticAction_InDealVtableOffset = 0x28; // bool (__thiscall*)(action): part of the deal (0xA64560)
	// the types (the same as Mac's: the ctors 0xBFCC00 and 0xBFC900 pass 6 and 12)
	constexpr int DiplomaticAction_TradeRegions = 6;
	constexpr int DiplomaticAction_Peace = 12;
	constexpr int DiplomaticAction_MakeTributary = 25;         // the dispatcher 0x14E9370 adds its term with 0x1522260

	// NEGOTIATION (ctor 0x8E2C40): the factions, its 28 actions (embedded, not indexed by type: trade regions is the
	// 5th) in a list { vtable, capacity, count, DIPLOMATIC_ACTION** } at + 0x508
	constexpr uint32_t Negotiation_FactionA = 0x10;            // FACTION*
	constexpr uint32_t Negotiation_FactionB = 0x14;            // FACTION*
	constexpr uint32_t Negotiation_ActionList = 0x508;
	constexpr uint32_t ActionList_Count = 0x08;
	constexpr uint32_t ActionList_Data = 0x0C;
	constexpr uint32_t Negotiation_ActionCount = 0x510;        // uint32
	constexpr uint32_t Negotiation_Actions = 0x514;            // DIPLOMATIC_ACTION**

	// the trade regions action (ctor 0xBFCC00, vtable 0x1B97AB0): regions in list 1 go to negotiation faction A, in list
	// 2 to faction B. void (__thiscall*)(DIPLOMATIC_ACTION_TRADE_REGIONS*) executes the deal (vtable slot 13)
	constexpr uint32_t TradeRegions_Execute = 0xC173E0;
	constexpr uint32_t TradeRegions_List1Count = 0x18;         // REGION** at + 0x1c
	constexpr uint32_t TradeRegions_List2Count = 0x24;         // REGION** at + 0x28
	// void (__thiscall*)(SETTLEMENT*, FACTION*, int32_t, int32_t): the deal's transfer of one region (reason 5, diplomacy)
	constexpr uint32_t Settlement_DiplomaticTransfer = 0x855870;
	constexpr uint32_t Region_Settlement = 0x128;              // SETTLEMENT*

	// Region value in the AI's deal evaluation
	// float (__cdecl*)(NEGOTIATION* deal, void* aiContext, CAI_FACTION* a, CAI_FACTION* b, int32_t* out, bool* out)
	constexpr uint32_t Diplo_EvaluateDeal = 0xA6E9B0;
	// float (__cdecl*)(ComponentList* components, void* aiContext, CAI_FACTION* a, void* context, int32_t*, char*, uint32_t):
	// scores the deal's CAI_DEAL_COMPONENT_* list (entries {component*, int32 action index}, 8 bytes, built by 0xA6E930)
	constexpr uint32_t Diplo_ScoreComponents = 0xA557D0;
	// void (__thiscall*)(CAI_DEAL_COMPONENT_GIFT* self, void* aiContext, CAI_FACTION* a, CAI_FACTION* b, int32_t gold), 0x1c bytes
	constexpr uint32_t DealComponentGift_ctor = 0xBF0FC0;
	constexpr uint32_t DealComponentGift_Size = 0x1C;
	// its score parts, float (__thiscall*)(GIFT*) virtuals (vtable 0x1B9929C, one dtor slot: Mac's slots - 0xc; + 0x8, the
	// table part 0xC804A0, needs the scorer's context)
	constexpr uint32_t DealComponentGift_BaseSlot = 0x4;      // 0xA7ECB0
	constexpr uint32_t DealComponentGift_SizeSlot = 0xC;      // 0xC29A80, the size class from the economic power
	constexpr uint32_t DealComponentGift_AmountSlot = 0x10;   // 0xC2D2E0, linear, over the paying faction's GDP
	constexpr uint32_t DealComponentGift_OtherSlot = 0x14;    // 0xA81F60
	// float (__thiscall*)(CAI_FACTION*, int32_t treasuryOffset): clamp(GDP / economic_power_gdp_factor + (treasury + offset)
	// / economic_power_treasury_factor, min, max); the CAI faction wrapper's FACTION is at + 0xec
	constexpr uint32_t CaiFaction_EconomicPower = 0x8FC320;
	constexpr uint32_t CaiFaction_Faction = 0xEC;
	constexpr uint32_t Treasury_Get = 0x86EB60;                // int32_t (__thiscall*)(TREASURY* = FACTION + 0x168), the Lua treasury
	constexpr uint32_t Faction_Treasury = 0x168;
	// uint32_t (__thiscall*)(FACTION*): the faction's GDP per turn (Mac's returns a float)
	constexpr uint32_t Faction_Gdp = 0x7107E0;
	using FactionGdpValue = uint32_t;

	// The region panel (DiplomacyTradeRegionsPanel, layout diplomacy_hud "trade_regions", set up by 0x15E6490)
	// void (__thiscall*)(PANEL* self, DIPLOMATIC_ACTION* action): fills list_offers (player -> AI) and list_demands (AI ->
	// player), the fill 0x1603DF0 skips invisible lists
	constexpr uint32_t TradeRegionsPanel_Populate = 0x15FDB80;
	constexpr uint32_t TradeRegionsPanel_Component = 0x4;      // UIComponent*
	constexpr uint32_t TradeRegionsPanel_RowTemplate = 0x44;   // "template_region_list_item" (+ 0x40 the DiplomacyDropdown)
	constexpr uint32_t TradeRegionsPanel_Selected = 0x48;      // UIComponent* row
	constexpr uint32_t TradeRegionsPanel_OtherFaction = 0x4C;  // FACTION* of the AI, set by Populate
	// void (__thiscall*)(PANEL* self, UIComponent* row): unticks every row, ticks `row` (may be null) as the one
	// selection, sets ok_trade_regions active / inactive and pans the camera to the row's region
	constexpr uint32_t TradeRegionsPanel_SelectRow = 0x1606DC0;
	// void (__thiscall*)(LISTENER* self = panel + 0x30, void** event): event[0] the clicked component (a row's checkbox:
	// its row at + 0x88), the byte at event + 4 is 1 if it's now ticked; also the OK and cancel buttons
	constexpr uint32_t TradeRegionsPanel_OnClick = 0x15E6590;
	constexpr uint32_t TradeRegionsPanel_ClickListener = 0x30;
	// UIComponent* (__cdecl*)(UIComponent* parent, const TempString* name, bool recursive)
	constexpr uint32_t UIComponent_FindChild = 0x139FEB0;
	// void (__thiscall*)(UIComponent*, bool), the Lua SetVisible
	constexpr uint32_t UIComponent_SetVisible = 0x13B6840;
	constexpr uint32_t UIComponent_ClickedRow = 0x88;
	// UIComponent: id TempString at + 0x7c, children count + 0x90 / UIComponent** + 0x94, checkbox state at + 0x184
	constexpr uint32_t UIComponent_Id = 0x7C;
	constexpr uint32_t UIComponent_ChildCount = 0x90;
	constexpr uint32_t UIComponent_Children = 0x94;
	constexpr uint32_t UIComponent_Checkbox = 0x184;
	constexpr uint32_t Checkbox_SetChecked = 0x13B6140;        // void (__thiscall*)(CHECKBOX*, bool, bool, bool)
	// geometry (the Lua bindings Position 0x1385210, Dimensions 0x1385BF0, MoveTo 0x1385260, Resize 0x1385790): position
	// x, y floats; the current state, its width and height floats; MoveTo virtual (float[2]); the flags that let Resize
	// change the width / height. Resize is the virtual + 0x4c (width, height, bool, bool), 0x13B13F0
	constexpr uint32_t UIComponent_PositionX = 0xA0;
	constexpr uint32_t UIComponent_State = 0xB4;
	constexpr uint32_t UIState_Width = 0x28;
	constexpr uint32_t UIComponent_MoveToVtableOffset = 0x58;
	constexpr uint32_t UIComponent_ResizeVtableOffset = 0x4C;
	constexpr uint32_t UIComponent_CanResize = 0xE8;
	// FACTION* (__thiscall*)(DIPLOMATIC_ACTION*): the action's two factions (the negotiation's A and B)
	constexpr uint32_t DiplomaticAction_FactionA = 0x523EA0;
	constexpr uint32_t DiplomaticAction_FactionB = 0xC70910;
	// FACTION* (__thiscall*)(CAMPAIGN* = *g_Campaign), CampaignUI_GetLocalPlayerFaction
	constexpr uint32_t Campaign_LocalFaction = 0x1521C30;
	constexpr uint32_t TempString_ToInt = 0xF2D10;             // bool (__thiscall*)(const TempString*, int32_t* out)
	constexpr uint32_t Region_Id = 0x4;                        // int32, the panel's row ids ("%d")

	// void (__thiscall*)(DIPLOMACY_CONTEXT*, int32_t regionId, FACTION* demandedFrom (null: offered), DIPLOMATIC_ACTION* deal):
	// the OK button (through 0x14F5C30, the context = DiplomacyDropdown + 0x184); fills deal terms with the one region
	// and applies them to the action
	constexpr uint32_t DiplomacyContext_SetRegion = 0x1522850;
	constexpr uint32_t DiplomacyContext_Negotiation = 0x80;
	// the deal terms argument: *(*(*(context + 0x78) + Campaign_Handle) + 0x2c)
	constexpr uint32_t DiplomacyContext_Campaign = 0x78;
	constexpr uint32_t ModelHandle_DealTermsContext = 0x2C;
	constexpr uint32_t DealTerms_ctor = 0xBFC030;              // void (__thiscall*)(TERMS*), 0x14c bytes
	constexpr uint32_t DealTerms_dtor = 0x6DB220;
	constexpr uint32_t DealTerms_SetDemandedRegions = 0xC27E40; // void (__thiscall*)(TERMS*, const List*), copied to + 0x10
	constexpr uint32_t DealTerms_SetOfferedRegions = 0xC58B90;  // void (__thiscall*)(TERMS*, const List*), copied to + 0x1c
	// void (__thiscall*)(DIPLOMATIC_ACTION*, void* x, TERMS*), a thunk to the action's virtual + 0x38
	constexpr uint32_t Deal_ApplyTerms = 0xC685F0;

	// The deal list row of a trade regions action (DiplomacyListTradeRegionsButton, made by 0x15E2470):
	// void (__thiscall*)(BUTTON* self, UIComponent* dropdownRoot, ENTRY* entry, void* context), retn 0xc; it shows the
	// demanded regions if there are any, else the offered ones, in "region_names"
	constexpr uint32_t DiplomacyListTradeRegionsButton_Setup = 0x15E2340;
	constexpr uint32_t ActionEntry_Action = 0x10;              // the entry's DIPLOMATIC_ACTION* (after a 12-byte WString and flags)
	constexpr uint32_t ListButton_Row = 0x4;                   // the button's row UIComponent*
	constexpr uint32_t Region_Owner = 0x124;                   // **(region + 0x124) = owning FACTION* (the Lua owning_faction 0x8560A0)
	constexpr uint32_t Region_LocalisedName = 0x94AE50;        // const WString* (__thiscall*)(REGION*)
	// WString (UTF-16, 12 bytes): ctor from a literal, append, append literal, non-empty, dtor
	constexpr uint32_t UniString_ctor = 0xDFEF0;
	constexpr uint32_t UniString_Append = 0xE1210;
	constexpr uint32_t UniString_AppendLiteral = 0xE12D0;
	constexpr uint32_t UniString_NotEmpty = 0xEF750;
	constexpr uint32_t UniString_dtor = 0xE0720;
	constexpr uint32_t UniString_Copy = 0xDFDF0;               // void (__thiscall*)(WString* self, const WString* other)
	constexpr uint32_t UniString_Size = 0xC;

	// REGION FOR TROOPS
	// The diplomacy's "Add Offer/Demand" list (DiplomacyDropdown, the controller at its component + 0x184): entries in a
	// vector at + 0xf8 ({ capacity, count, ENTRY* }, 0x14-byte entries: WString tooltip, two flags at + 0xc / + 0xd, the
	// action at + 0x10), rebuilt by 0x14ED890 (its push is inlined: ActionEntry_Alloc, WString copy, tw_free).
	// void (__thiscall*)(DROPDOWN*) 0x14F6E90 makes a button "diplomac_action_possible_<type>" in the list box at + 0x98
	// (template + 0x9c) for each entry whose action isn't in the deal, and hands it the entry (0x15FE4C0: the action at
	// controller + 0x34, label "row_tx" from the action's name, tooltip from the entry)
	constexpr uint32_t DiplomacyDropdown_MakeActionButtons = 0x14F6E90;
	constexpr uint32_t DiplomacyDropdown_ActionEntries = 0xF8;
	constexpr uint32_t DiplomacyDropdown_ActionButtonParent = 0x98;
	constexpr uint32_t DiplomacyDropdown_Context = 0x128;      // DIPLOMACY_CONTEXT*, its negotiation at + 0x80; + 0x138 the open action
	constexpr uint32_t ActionEntry_Alloc = 0xBE440;            // ENTRY* (__cdecl*)(int32_t count), tw_malloc
	// A click on an action button: void (__thiscall*)(LISTENER* = dropdown + 0x40, EVENT*), the event's button at + 4
	// (its controller + 0x34 the action), a click when + 8 is 1; then 0x14E9370(dropdown, 0, action) opens the action's
	// panel (type 6: the region panel, 0x14DE560). There's no second entry like Mac's thunk.
	constexpr uint32_t DiplomacyDropdown_OnActionButton = 0x14A54E0;
	constexpr uint32_t DiplomacyDropdown_ActionButtonListener = 0x40;
	constexpr uint32_t ActionButtonEvent_Button = 0x4;
	constexpr uint32_t ActionButtonEvent_Clicked = 0x8;
	constexpr uint32_t ActionButtonController_Action = 0x34;
	// void (__thiscall*)(DIPLOMACY_CONTEXT*, DIPLOMATIC_ACTION*): puts an action into the deal with empty terms (the
	// dispatcher 0x14E9370's way for simple terms); the tributary term the way the "Make Tributary State" button adds it
	constexpr uint32_t DiplomacyContext_AddTerm = 0x15221C0;
	constexpr uint32_t DiplomacyContext_AddTributaryTerm = 0x1522260;
	// The troops list's rows: UIComponent* (__cdecl*)(UIComponent* listBox, void* rowTemplate, const TempString* id, void*,
	// void*, int32_t, int32_t, void*, bool, void*), as the panel's fill calls it; void (__thiscall*)(listBox) removes all
	// rows / lays the list out again
	constexpr uint32_t UIComponent_CreateListRow = 0x13A05C0;
	constexpr uint32_t ListBox_Clear = 0x13A0FB0;
	constexpr uint32_t ListBox_Layout = 0x139F120;

} // namespace sdk::Attila::Addresses

#pragma once

#include "Build.hpp"

// Unslid addresses (image base 0x100000000), stored as {Steam RC2, App Store RC4}.
// Selected by exact UUID before hooks initialize. Evidence: docs/macos/addresses.md.
namespace sdk::Attila::Addresses
{

// STARTUP
// int64_t (*)(void* instance, void* prevInstance, const char16_t* commandLine), CA's WinMain on the WinMain thread
constexpr BuildAddress WinMain{0x1032bc10c, 0x1032baa54};

// GAME LOOP
// bool (*)(void* app, int, int, int, int), called once per frame by the run loop on the WinMain (game/Lua) thread
constexpr BuildAddress GameTick{0x1010ecd24, 0x1010eb66c};

// LUA LOG
// void (*)(const char*), called by print, out and the Lua error handler, falls back to fputs(stdout) if null
constexpr BuildAddress g_LuaLogSink{0x1056d6588, 0x1056d68f8};

// void (*)(void (*sink)(const char*)), the str instruction that stores the sink
constexpr BuildAddress SetLuaLogSink_Store{0x103f25dec, 0x103f24734};

// SCRIPT RUNTIMES
// Linked list of all active lua states
constexpr BuildAddress g_RuntimeLuaListHead{0x10560b7c8, 0x10560bb38};
constexpr BuildAddress g_RuntimeLuaListSentinel{0x10560b7d0, 0x10560bb40};

// lua_State* (*)(void* scriptInterface)
constexpr BuildAddress GetLuaState{0x1038b377c, 0x1038b20c4};

// MOD LOADER
// void* (*)(ScriptingEnv* self), EPISODIC_SCRIPTING_ENV virtual slot 3, loads and runs campaigns/<name>/scripting.lua
constexpr BuildAddress RunStartupPath{0x102170104, 0x10216ea4c};

// VFS
// void* (*)(), returns the static VFS instance
constexpr BuildAddress VFS_GetInstance{0x10387c0a8, 0x10387a9f0};

// void (*)(void* vfs, CName* dir, CName* pattern, VFSSearchResults* out, int flags, int mode), a virtual of the VFS
constexpr uint64_t VFS_SearchFiles_VtableOffset = 0x58;

// CName* (*)(CName* self, const char* str)
constexpr BuildAddress CName_ctor{0x10109240c, 0x101090d54};

// void (*)(void* ptr), the game allocator's free
constexpr BuildAddress tw_free{0x1010acb70, 0x1010ab4b8};
constexpr BuildAddress tw_malloc{0x1010aca70, 0x1010ab3b8};

// DIPLOMACY
// float (*)(void* deal, void* ctx, void* factionA, void* factionB)
// UI-only wrapper around the deal evaluator (0x101d7b25c), AI accepts deals with score >= 0
constexpr BuildAddress Diplo_GetDisplayedDealScore{0x101d7e890, 0x101d7d1d8};

// int (*)(void* caiModule, void* deal)
// buckets the score: 2 low, 3 moderate, 4 high, thresholds are the tweakers
// CAI_DIPLOMACY_NEGOTIATION_DISPLAYED_LIKELIHOOD_THRESHOLD_LOW/HIGH (-4 / 4, value at +0x64 of 0x105600378 / 0x1056003e0)
constexpr BuildAddress CAI_GetDealLikelihoodBucket{0x102296a5c, 0x1022953a4};

// void* (*)(DiplomacyDropdown* self, int likelihood (-1/0/1, -2 hidden), bool show)
constexpr BuildAddress DiplomacyDropdown_SetLikelihood{0x1035f4a1c, 0x1035f3364};

// UI
// void (*)(UIComponent* self, const WString* text, bool allStates)
constexpr BuildAddress UIComponent_SetTooltipText{0x1032d2418, 0x1032d0d60};

// STRINGS
// TempString* (*)(TempString* self, const char* utf8)
constexpr BuildAddress String_ctor{0x10104ce1c, 0x10104b764};

// void (*)(TempString* self)
constexpr BuildAddress String_dtor{0x10104ba58, 0x10104a3a0};

// WStringResult (*)(const TempString* src), returns through x8
constexpr BuildAddress String_ToUniString{0x101049bb0, 0x1010484f8};

// void (*)(WString* self)
constexpr BuildAddress UniString_dtor{0x101049d20, 0x101048668};

// CAMPAIGN MODEL (for the twase.* Lua API)
// bool (*)(lua_State* L, int idx, FACTION_SCRIPT_INTERFACE** out), the game's own faction argument check:
// 0 if the value isn't a userdata, a Lua type error if it's another userdata type
constexpr BuildAddress FactionScriptInterface_GetArg{0x101d5c640, 0x101d5af88};

// int (*)(CAI_INTERFACE* self, const FACTION& a, const FACTION& b), EMPIRECAMPAIGNAI::CAI_INTERFACE::get_faction_attitude:
// sums the attitude components, rounded. Found via the RTTI of its lambda (vtable 0x104c12268)
constexpr BuildAddress CAI_GetFactionAttitude{0x10229d814, 0x10229c15c};
using FactionAttitudeValue = int;

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
constexpr BuildAddress g_Campaign{0x105608ea0, 0x105609210};
constexpr uint64_t Campaign_Handle = 0x30;
constexpr uint64_t World_Database = 0x21E0;                // DB_GetContainer(world + 0x21e0)
// void* (*)(void** worldDatabaseField) = *(*(*field + 0x58) + 0x20), the DB container
constexpr BuildAddress DB_GetContainer{0x101e76780, 0x101e750c8};
// DB table accessors: DATABASE_TABLE* (*)(void* container), load the table on first use ("Loading database: %s")
constexpr BuildAddress DB_BuildingLevels{0x102c86dfc, 0x102c85744};        // container + 0x1fb0
constexpr BuildAddress DB_RegionToProvinceJunctions{0x102cc5c08, 0x102cc4550}; // container + 0x1630
constexpr BuildAddress DB_Provinces{0x102c8ea88, 0x102c8d3d0};             // container + 0x1638
constexpr BuildAddress DB_MainUnits{0x102c90940, 0x102c8f288};             // container + 0x14d8
// the levy pools: faction -> mercenary set, set -> unit groups, group -> unit (layouts in docs/addresses.md)
constexpr BuildAddress DB_FactionToMercenarySetJunctions{0x102cc6b40, 0x102cc5488};
constexpr BuildAddress DB_MercenaryPoolToGroupsJunctions{0x102cc71b8, 0x102cc5b00};
constexpr BuildAddress DB_MercenaryUnitGroups{0x102cc6e98, 0x102cc57e0};
// int* (*)(StringHashMap* map, const TempString* key), the record index or null
constexpr BuildAddress StringHashMap_Find{0x1010e5040, 0x1010e3988};

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
// levy pools (twase.levy_pool): references to other tables are record pointers
constexpr uint64_t MercenarySet_Faction = 0x00;            // faction_to_mercenary_set_junctions: factions record*
constexpr uint64_t MercenarySet_Pool = 0x08;               // mercenary pool record*
constexpr uint64_t PoolGroup_Pool = 0x08;                  // mercenary_pool_to_groups_junctions: mercenary pool record*
constexpr uint64_t PoolGroup_Group = 0x10;                 // mercenary_unit_groups record*
constexpr uint64_t PoolGroup_InitialCount = 0x18;          // int32 initial_unit_count
constexpr uint64_t MercenaryGroup_ChanceToReplenish = 0x00; // mercenary_unit_groups: float
constexpr uint64_t MercenaryGroup_Key = 0x08;              // TempString
constexpr uint64_t MercenaryGroup_MaxCount = 0x18;         // int32
constexpr uint64_t MercenaryGroup_MaxReplenishPerTurn = 0x1C; // int32
constexpr uint64_t MercenaryGroup_Unit = 0x20;             // main_units record*

// the diplomacy screen's actions (twase.dev_diplomacy_actions)
constexpr BuildAddress DiplomacyDropdown_Vtable{0x104cc6a38, 0x104cc68d8}; // UIDLL::DiplomacyDropdown (RTTI typeinfo 0x104cc6d38)
// DiplomacyDropdown_Context, DiplomacyContext_Negotiation and DiplomaticAction_Type: see NATIVE REGION TRADING below
constexpr uint64_t Negotiation_ActionList = 0x700;         // list: count + 0xc, DIPLOMATIC_ACTION** + 0x10 (= Negotiation_ActionCount / _Actions)
constexpr uint64_t ActionList_Count = 0x0C;
constexpr uint64_t ActionList_Data = 0x10;
constexpr uint64_t DiplomaticAction_Side = 0x10;           // int32, 1 offer
constexpr uint64_t DiplomaticAction_Flags = 0x18;          // uint8 available, uint8 blocked

// NATIVE REGION TRADING ("Gift Region", see Hooks/NativeRegionTrading.cpp)
// RegionList (*)(FACTION* from, FACTION* to) through x8: from's regions that can be given, not the capital, no garrisoned
// army / navy; with `to`, only regions where an army of `to` stands (the horde settling rule). Used by the region panel
// (0x1035adce8: offers from -> to, demands to -> from) and by the availability rule below
constexpr BuildAddress Faction_GiftableRegions{0x101bdad20, 0x101bd9668};
// bool (*)(FACTION* a, FACTION* b), is the trade regions action (type 6) available: one side human, the other a horde,
// and giftable regions exist (called by the action's availability update 0x1025c79a0)
constexpr BuildAddress DiplomaticAction_RegionTradePossible{0x1025c7a6c, 0x1025c63b4};
constexpr uint64_t Faction_IsHuman = 0x970;                // bool (the Lua is_human)
constexpr uint64_t Faction_RegionCount = 0xA4C;            // u32, the regions at +0xa50 (the Lua region_list 0x101d2a614)
constexpr uint64_t Faction_CanOwnLand = 0x1751;            // bool, 0 for hordes that can't settle (0x101bec634; needed by vanilla's Gift Region to a horde and transfer_region_to_faction)
constexpr BuildAddress Faction_IsHorde{0x101b521ec, 0x101b50b34};          // void* (*)(FACTION*), non-null for hordes (the Lua is_horde)
using FactionIsHordeValue = void*;

// The native region panel (UIDLL::DiplomacyTradeRegionsPanel, layout diplomacy_hud "trade_regions")
// void (*)(PANEL* self, DIPLOMATIC_ACTION* action): fills list_offers (player -> AI) and list_demands (AI -> player)
constexpr BuildAddress TradeRegionsPanel_Populate{0x1035ad948, 0x1035ac290};
constexpr uint64_t TradeRegionsPanel_Component = 0x8;      // UIComponent*
// UIComponent* (*)(UIComponent* parent, const TempString* name, bool recursive)
constexpr BuildAddress UIComponent_FindChild{0x1032d4f88, 0x1032d38d0};
// void (*)(UIComponent*, bool), the engine side of the Lua SetVisible
constexpr BuildAddress UIComponent_SetVisible{0x1032c6b00, 0x1032c5448};
// void (*)(PANEL* self, UIComponent* row): unticks every row of both lists, ticks `row` (may be null) and keeps it as
// the one selection, sets ok_trade_regions active / inactive and pans the camera to the row's region
constexpr BuildAddress TradeRegionsPanel_SelectRow{0x1035adff8, 0x1035ac940};
constexpr uint64_t TradeRegionsPanel_Selected = 0x78;      // UIComponent* row
// uint64_t (*)(PANEL* self, void** event): event[0] the clicked component (a row's checkbox: its row at + 0x108), the
// byte at event + 8 is 1 if the checkbox is now ticked (the rest of that word is garbage); also the OK and cancel buttons
constexpr BuildAddress TradeRegionsPanel_OnClick{0x1035ae4c4, 0x1035ace0c};
constexpr uint64_t UIComponent_ClickedRow = 0x108;
// The panel's list rows: UIComponent* (*)(UIComponent* listBox, void* rowTemplate (= panel + 0x70), const TempString* id,
// void*, void*, int32_t, int32_t, void*, bool (stack, true), void* (stack)), as the fill function 0x1035adce8 calls it;
// it then sets the row's text (UIComponent_SetText). 0x1032d25b4(listBox) removes all rows, 0x1032bc860(listBox) lays
// the list out again.
constexpr BuildAddress UIComponent_CreateListRow{0x1032ef074, 0x1032ed9bc};
constexpr uint64_t TradeRegionsPanel_RowTemplate = 0x70;
constexpr BuildAddress ListBox_Clear{0x1032d25b4, 0x1032d0efc};
constexpr BuildAddress ListBox_Layout{0x1032bc860, 0x1032bb1a8};

// The diplomacy's "Add Offer/Demand" list: entries in a vector at DiplomacyDropdown + 0x1c0 ({ capacity, count,
// ENTRY* }, 0x20-byte entries: UniString text (the tooltip), two flags at + 0x10 / + 0x11, the action at + 0x18), built by
// 0x1036cca88; int64_t (*)(DROPDOWN*) 0x1035f4538 makes a button "diplomac_action_possible_<type>" under dropdown + 0x118
// (template dropdown + 0x120) for each entry whose action isn't in the deal, and hands it the entry (0x1035a9880:
// action at controller + 0x50, label "row_tx" from the action's name, tooltip from the entry).
constexpr uint64_t DiplomacyDropdown_ActionEntries = 0x1C0;
constexpr uint64_t DiplomacyDropdown_Context = 0x200;       // DIPLOMACY_CONTEXT*, its negotiation at + 0xd0
constexpr uint64_t DiplomacyDropdown_ActionButtonParent = 0x118;
constexpr BuildAddress DiplomacyDropdown_MakeActionButtons{0x1035f4538, 0x1035f2e80};
constexpr BuildAddress Vector_PushActionEntry{0x103200f00, 0x1031ff848};   // void (*)(VECTOR*, const ENTRY*), copies the entry
constexpr BuildAddress UniString_Copy{0x10104a138, 0x101048a80};           // void (*)(UniString* self, const UniString* other)
// A click on an action button: void (*)(DROPDOWN*, EVENT*), event + 8 the button (its controller + 0x50 the action),
// then 0x1035f5044(dropdown, 0, action) opens the action's panel (type 6: the region panel, 0x1035f6b88). 0x1035f5290 is
// the same through the dropdown's other base (this = dropdown + 0x68).
constexpr BuildAddress DiplomacyDropdown_OnActionButton{0x1035f4fe8, 0x1035f3930};
constexpr uint64_t ActionButtonEvent_Button = 0x8;         // the event's button UIComponent*
constexpr uint64_t ActionButtonEvent_Clicked = 0x10;       // uint8, 1 for a click
constexpr uint64_t ActionButtonController_Action = 0x50;   // the button's controller (+ UIComponent_Checkbox): its action
constexpr uint64_t UniString_Size = 0x10;
constexpr BuildAddress DiplomacyDropdown_OnActionButtonThunk{0x1035f5290, 0x1035f3bd8};
// void (*)(DIPLOMACY_CONTEXT*, DIPLOMATIC_ACTION*): puts an action into the deal with empty terms (the dispatcher's way
// for simple terms); used for peace and tributary
constexpr BuildAddress DiplomacyContext_AddTerm{0x1036cbadc, 0x1036ca424};
// void (*)(DIPLOMACY_CONTEXT*, DIPLOMATIC_ACTION*): the tributary term the way the "Make Tributary State" button adds it
// (terms with 0x1024d5b54: the other side becomes your tributary; the generic add leaves the direction empty, and the
// game then makes you the tributary)
constexpr BuildAddress DiplomacyContext_AddTributaryTerm{0x1036cba5c, 0x1036ca3a4};
constexpr int DiplomaticAction_Peace = 12;               // "Peace Treaty"
constexpr int DiplomaticAction_MakeTributary = 25;       // "Make Tributary State" (not available at war)
constexpr uint64_t DiplomaticAction_Available = 0x18;    // bool, + 0x19 blocked
constexpr uint64_t TradeRegionsPanel_OtherFaction = 0x80;  // FACTION* of the AI, set by Populate
// FACTION* (*)(DIPLOMATIC_ACTION*): the action's two factions (Populate takes the one that isn't the local faction)
constexpr BuildAddress DiplomaticAction_FactionA{0x1024c829c, 0x1024c6be4};
constexpr BuildAddress DiplomaticAction_FactionB{0x1025d2180, 0x1025d0ac8};
// void (*)(DIPLOMACY_CONTEXT*, int32_t regionId, FACTION* demandedFrom (null: offered), DEAL* deal): the OK button;
// fills deal terms with the one region and applies them to the deal (replacing the region term)
constexpr BuildAddress DiplomacyContext_SetRegion{0x1036cb868, 0x1036ca1b0};
constexpr BuildAddress DealTerms_ctor{0x1024c8130, 0x1024c6a78};           // void (*)(TERMS*), about 0x1b0 bytes
constexpr BuildAddress DealTerms_dtor{0x101a82704, 0x101a8104c};           // void (*)(TERMS*)
constexpr BuildAddress DealTerms_SetDemandedRegions{0x1025c7df4, 0x1025c673c}; // void (*)(TERMS*, const List*), copied to + 0x10
constexpr BuildAddress DealTerms_SetOfferedRegions{0x1025c7f00, 0x1025c6848};  // void (*)(TERMS*, const List*), copied to + 0x20
// void (*)(DEAL*, void* x, TERMS*), a thunk to the deal's virtual + 0x78; x = *(*(*(context + 0xc0) + 0x30) + 0x50)
constexpr BuildAddress Deal_ApplyTerms{0x1025d2124, 0x1025d0a6c};
// void (*)(DIPLOMATIC_ACTION_TRADE_REGIONS*): executes the deal, list 1 to negotiation faction A, list 2 to faction B
// (vtable 0x104c1e520 slot 13)
constexpr BuildAddress TradeRegions_Execute{0x1025c8188, 0x1025c6ad0};
constexpr uint64_t DiplomaticAction_Negotiation = 0x8;
// void (*)(SETTLEMENT*, FACTION*, int64_t, int32_t): the deal's transfer of one region (reason 5, diplomacy)
constexpr BuildAddress Settlement_DiplomaticTransfer{0x101ceb290, 0x101ce9bd8};
constexpr uint64_t Region_Settlement = 0x1A8;              // SETTLEMENT*
// The deal list row of a trade regions action (UIDLL::DiplomacyListTradeRegionsButton, vtable slot + 0x1e0):
// void (*)(BUTTON* self, UIComponent* dropdownRoot, ENTRY* entry, void* context); entry + 0x18 = the action.
// It shows the demanded regions if there are any, else the offered ones, in "region_names".
constexpr BuildAddress DiplomacyListTradeRegionsButton_Setup{0x1035a6eec, 0x1035a5834};
constexpr uint64_t ActionEntry_Action = 0x18;              // the entry's DIPLOMATIC_ACTION*
constexpr uint64_t ListButton_Row = 0x8;                   // the button's row UIComponent*
constexpr uint64_t Region_Owner = 0x1A0;                   // **(region + 0x1a0) = owning FACTION* (the Lua owning_faction)
constexpr BuildAddress Region_LocalisedName{0x101c57700, 0x101c56048};     // const UniString* (*)(REGION*)
// UniString (UTF-16, 0x10 bytes): ctor from a u"" literal, append, append literal, non-empty, set component text
constexpr BuildAddress UniString_ctor{0x101049d84, 0x1010486cc};
constexpr BuildAddress UniString_Append{0x1010537fc, 0x101052144};
constexpr BuildAddress UniString_AppendLiteral{0x1010535fc, 0x101051f44};
constexpr BuildAddress UniString_NotEmpty{0x1010493b4, 0x101047cfc};
constexpr BuildAddress UIComponent_SetText{0x1032c918c, 0x1032c7ad4};      // void (*)(UIComponent*, const UniString*, bool)
constexpr BuildAddress Campaign_LocalFaction{0x1036c2c94, 0x1036c15dc};    // FACTION* (*)(CAMPAIGN* = *g_Campaign)
constexpr BuildAddress TempString_ToInt{0x10109896c, 0x1010972b4};         // bool (*)(const TempString*, int32_t* out)
constexpr uint64_t Region_Id = 0x8;                        // int32, the panel's row ids ("%d")
// UIComponent: id TempString at + 0xf8, children count + 0x114 / UIComponent** + 0x118, checkbox state at + 0x258
constexpr uint64_t UIComponent_Id = 0xF8;
constexpr uint64_t UIComponent_ChildCount = 0x114;
constexpr uint64_t UIComponent_Children = 0x118;
constexpr uint64_t UIComponent_Checkbox = 0x258;
// geometry (the Lua bindings Position, Dimensions, MoveTo, Resize): position x, y floats; the current state, its width
// and height floats; MoveTo virtual (float[2]); the flags that let Resize change the width / height. Resize is the
// virtual + 0xa8 (bool, bool, width, height), see Hooks/NativeRegionTradingDetours.cpp
constexpr uint64_t UIComponent_PositionX = 0x128;
constexpr uint64_t UIComponent_State = 0x140;
constexpr uint64_t UIState_Width = 0x40;
constexpr uint64_t UIComponent_MoveToVtableOffset = 0xC8;
constexpr uint64_t UIComponent_CanResize = 0x190;
// SetRegion's deal terms argument: *(*(*(context + 0xc0) + Campaign_Handle) + 0x50)
constexpr uint64_t DiplomacyContext_Campaign = 0xC0;
constexpr uint64_t ModelHandle_DealTermsContext = 0x50;
constexpr BuildAddress Checkbox_SetChecked{0x1033090c0, 0x103307a08};      // void (*)(CHECKBOX*, bool, bool, bool)

// Gift sizes: float (*)(CAI_FACTION*, int32_t treasuryOffset) = clamp(GDP / economic_power_gdp_factor + (treasury +
// offset) / economic_power_treasury_factor, economic_power_min, economic_power_max); a gift is small / medium / large
// from gift_base_value_* x this (0x101d7ef3c). The CAI faction wrapper's FACTION is at + 0x150.
constexpr BuildAddress CaiFaction_EconomicPower{0x1019e1568, 0x1019dfeb0};
constexpr uint64_t CaiFaction_Faction = 0x150;
constexpr BuildAddress Treasury_Get{0x101cb57f4, 0x101cb413c};             // int32_t (*)(TREASURY* = FACTION + 0x250)
// float (*)(FACTION*): the faction's GDP per turn. The gift's linear score part (vtable + 0x28, 0x101d76bb4) is the
// amount within / beyond the AI's budget, weighted by its personality, over the paying faction's GDP, capped.
constexpr BuildAddress Faction_Gdp{0x101b70018, 0x101b6e960};
using FactionGdpValue = float;
constexpr uint64_t Faction_Treasury = 0x250;

// Region value in the AI's deal evaluation
// float (*)(NEGOTIATION* deal, void* aiContext, FACTION* a, FACTION* b, int32_t* out, bool* out)
constexpr BuildAddress Diplo_EvaluateDeal{0x101d7b25c, 0x101d79ba4};
// float (*)(ComponentList* components, void* aiContext, FACTION* a, ...): scores the deal's CAI_DEAL_COMPONENT_* list
// (entries {component*, int32 index}, 16 bytes); called once at the end of Diplo_EvaluateDeal
constexpr BuildAddress Diplo_ScoreComponents{0x101d7e26c, 0x101d7cbb4};
// void (*)(CAI_DEAL_COMPONENT_GIFT* self, void* aiContext, FACTION* a, FACTION* b, int32_t gold), 0x30 bytes;
// the evaluator builds it for Payments with gold = -(payment amount)
constexpr BuildAddress DealComponentGift_ctor{0x1024f485c, 0x1024f31a4};
constexpr uint64_t DealComponentGift_Size = 0x30;
// its score parts, float (*)(GIFT*) virtuals (vtable 0x104c1a2c0; + 0x18, the table part, needs the scorer's context)
constexpr uint64_t DealComponentGift_BaseSlot = 0x10;
constexpr uint64_t DealComponentGift_SizeSlot = 0x20;     // the size class, from the economic power
constexpr uint64_t DealComponentGift_AmountSlot = 0x28;   // linear, over the paying faction's GDP
constexpr uint64_t DealComponentGift_OtherSlot = 0x30;
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

// FERAL MOD MANAGER (CModManager, see docs/macos/addresses.md)
// uint64_t (*)(CModManager*), links the entries' packs into VFS/Local/mods; only FeralAppendModsToCommandLine
// (0x100014f1c) calls it, after the DisableAllMods check and its 1 s wait for the Workshop details
constexpr BuildAddress CModManager_LinkPacks{0x100014b34, 0x100014b34};
// void* (*)(CModManager*), sorts the entries (enabled first, then by order) and closes gaps in the order
constexpr BuildAddress CModManager_SortByOrder{0x1000146d8, 0x1000146d8};
// const char* (*)(const NString*), UTF-8 (encoding 1, cached in the string)
constexpr BuildAddress NString_CStr{0x1002d404c, 0x1002d3bd8};
constexpr uint64_t CModManager_Refreshing = 0x09;   // bool, until all Workshop details (and their preferences) are in
constexpr uint64_t CModManager_EntriesBegin = 0x18; // entries of CModManager_EntrySize bytes
constexpr uint64_t CModManager_EntriesEnd = 0x20;
constexpr uint64_t CModManager_EntrySize = 0x26A0;  // + 0x00 NString file name, + 0x58 int32 order, + 0x69 bool enabled
constexpr uint64_t ModEntry_Order = 0x58;
constexpr uint64_t ModEntry_Enabled = 0x69;

// CAMPAIGN UI (Steam arm64; evidence in docs/macos/addresses.md)
// Real writable const char* slot, loaded by both encyclopedia URL builders.
constexpr BuildAddress g_EncyclopediaBaseUrl{0x104ff0e48, 0x104ff1178};
constexpr BuildAddress Encyclopedia_NativeUrl{0x104892dde, 0x10489162e};
// void* (*)(ENCYCLOPEDIA_PAGES*), may replace the slot from Feral's preference.
constexpr BuildAddress Encyclopedia_ctor{0x10287e7f0, 0x10287d138};
constexpr BuildAddress Encyclopedia_UrlLoadEntry{0x10287eb78, 0x10287d4c0};
constexpr BuildAddress Encyclopedia_UrlLoadPage{0x10287ecd4, 0x10287d61c};
// intptr_t (*)(CampaignSettlementCallback*, REGION*), initializes building slot controllers.
constexpr BuildAddress CampaignSettlement_Initialize{0x103457800, 0x103456148};
// void* (*)(CampaignBuildingSlot*), clears dynamic contents and its building pointer (also called by native refresh).
constexpr BuildAddress CampaignBuildingSlot_Clear{0x10345b30c, 0x103459c54};
constexpr uint64_t CampaignSettlement_Component = 0x08;
constexpr uint64_t CampaignSettlement_IsCapital = 0x70; // province-capital card, NOT currently selected region
constexpr uint64_t CampaignSettlement_MaxSlots = 0x74;  // int32, native 4 or 6

// PATCHES
constexpr BuildAddress BitSetCrashAddr{0x101a51c58, 0x101a505a0};

} // namespace sdk::Attila::Addresses

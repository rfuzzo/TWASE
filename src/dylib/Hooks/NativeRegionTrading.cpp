#include "stdafx.hpp"

#include "NativeRegionTrading.hpp"

#include "../Image.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/VFSTypes.hpp"

#include "../../sdk/Attila/Addresses.hpp"

// Attila's diplomacy has a "Gift Region" option (diplomatic action type 6, permission "regions"), but only to settle a
// horde: the action is available when one side is a horde, and only regions where the horde's army stands can be given.
// With region trading enabled we widen both rules for two settled factions:
// - available: also between a human and a non-horde AI faction, if the human has a region to give
// - regions: between a human and a non-horde, either side can give any region but its capital and garrisoned
//   settlements (offers and demands; the engine only lists regions where the receiver's army stands)
// The panel only lets you tick one region (in either list); we let you tick any number in both, so one deal can give
// and demand regions (a swap), and OK puts them all into the deal.
//
// The AI's deal evaluation has no region component (a region scores +0.0). For deals with regions we add the score the
// region is worth: the mirror image of the AI paying the region's value in gold (the native gift component Payments use),
// with the value from the Lua handler of the region trading mod.

using namespace sdk::Attila;

namespace
{
bool isAttached = false;
std::atomic<bool> isEnabled = false;

// the engine's region list, returned by value through x8 (the destructor keeps clang on that convention)
struct RegionList
{
    uint32_t capacity;
    uint32_t count;
    void* data;
    ~RegionList() {}
};

using IsHorde_t = void* (*)(const void* faction);

lua_State* luaState = nullptr;
std::atomic<int> valueSign = 1;

struct EvalContext
{
    void* negotiation;
    void* aiContext;
    void* factionA;
    void* factionB;
};
thread_local EvalContext* currentEval = nullptr;

struct ComponentEntry
{
    void* component;
    int32_t index;
    int32_t pad;
};
static_assert(sizeof(ComponentEntry) == 16);

struct ComponentList
{
    uint32_t capacity;
    uint32_t count;
    ComponentEntry* data;
};

using GiftCtor_t = void (*)(void* self, void* aiContext, void* a, void* b, int32_t gold);
using FindChild_t = void* (*)(void* parent, const TempString* name, bool recursive);
using SetVisible_t = void (*)(void* component, bool visible);
using StringCtor_t = TempString* (*)(TempString* self, const char* utf8);
using StringDtor_t = void (*)(TempString* self);
using InDeal_t = bool (*)(void* action);

RegionList GiftableRegions(void* from, void* to);
float EvaluateDeal(void* negotiation, void* aiContext, void* a, void* b, int32_t* out5, char* out6);
void* PopulatePanel(void* panel, void* action);
Hook<decltype(&PopulatePanel)> PopulatePanel_fnc(Addresses::TradeRegionsPanel_Populate, &PopulatePanel);
float ScoreComponents(ComponentList* list, void* aiContext, void* a, int64_t* a4, int32_t* a5, char* a6, uint64_t a7);
Hook<decltype(&EvaluateDeal)> EvaluateDeal_fnc(Addresses::Diplo_EvaluateDeal, &EvaluateDeal);
Hook<decltype(&ScoreComponents)> ScoreComponents_fnc(Addresses::Diplo_ScoreComponents, &ScoreComponents);
bool RegionTradePossible(void* a, void* b);
using SelectRow_t = void* (*)(void* panel, void* row);
uint64_t OnClick(void* panel, void** event);
Hook<decltype(&OnClick)> OnClick_fnc(Addresses::TradeRegionsPanel_OnClick, &OnClick);
void SetRegion(void* context, int32_t regionId, void* demandedFrom, void* deal);
Hook<decltype(&SetRegion)> SetRegion_fnc(Addresses::DiplomacyContext_SetRegion, &SetRegion);
void Execute(void* action);
Hook<decltype(&Execute)> Execute_fnc(Addresses::TradeRegions_Execute, &Execute);
void ListButtonSetup(void* button, void* root, void* entry, void* context);
Hook<decltype(&ListButtonSetup)> ListButtonSetup_fnc(Addresses::DiplomacyListTradeRegionsButton_Setup, &ListButtonSetup);

Hook<decltype(&GiftableRegions)> GiftableRegions_fnc(Addresses::Faction_GiftableRegions, &GiftableRegions);
Hook<decltype(&RegionTradePossible)> RegionTradePossible_fnc(Addresses::DiplomaticAction_RegionTradePossible, &RegionTradePossible);

bool IsHuman(const void* faction)
{
    return faction && static_cast<const uint8_t*>(faction)[Addresses::Faction_IsHuman] == 1;
}

bool IsHorde(const void* faction)
{
    static const auto isHorde = reinterpret_cast<IsHorde_t>(Addresses::Faction_IsHorde + Image::Get()->GetSlide());
    return faction && isHorde(faction) != nullptr;
}

// a human and a settled AI faction (no hordes, those keep the engine's rules)
bool IsSettledPair(const void* human, const void* other)
{
    return IsHuman(human) && other && !IsHuman(other) && !IsHorde(human) && !IsHorde(other);
}

RegionList GiftableRegions(void* from, void* to)
{
    if (isEnabled && to && (IsSettledPair(from, to) || IsSettledPair(to, from)))
    {
        return GiftableRegions_fnc(from, nullptr);
    }
    return GiftableRegions_fnc(from, to);
}

bool RegionTradePossible(void* a, void* b)
{
    if (RegionTradePossible_fnc(a, b))
    {
        return true;
    }
    if (!isEnabled)
    {
        return false;
    }
    void* human = IsHuman(a) ? a : (IsHuman(b) ? b : nullptr);
    void* other = human == a ? b : a;
    if (!human || !IsSettledPair(human, other))
    {
        return false;
    }
    RegionList regions = GiftableRegions_fnc(human, nullptr);
    const bool possible = regions.count > 0;
    if (regions.data)
    {
        reinterpret_cast<void (*)(void*)>(Addresses::tw_free + Image::Get()->GetSlide())(regions.data);
    }
    return possible;
}
const char* KeyOf(const void* object, uint64_t aRecordOffset)
{
    const void* record = object ? *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(object) + aRecordOffset) : nullptr;
    const auto* key = static_cast<const TempString*>(record);
    return key && key->length > 0 ? key->data : nullptr;
}

// The Lua handler's gold value of `region` for `faction` (0 without a handler)
float HandlerValue(const char* region, const char* faction)
{
    lua_State* L = luaState;
    if (!L || !region || !faction)
    {
        return 0.0f;
    }
    const int top = LuaRuntime::gettop(L);
    LuaRuntime::getfield(L, LUA_REGISTRYINDEX, "twase_region_value_handler");
    if (LuaRuntime::type(L, -1) != LUA_TFUNCTION)
    {
        LuaRuntime::settop(L, top);
        return 0.0f;
    }
    LuaRuntime::pushstring(L, region);
    LuaRuntime::pushstring(L, faction);
    float value = 0.0f;
    if (LuaRuntime::pcall(L, 2, 1, 0) != 0)
    {
        spdlog::error("[RegionTrading] value handler failed: {}", LuaRuntime::tolstring(L, -1, nullptr));
    }
    else if (LuaRuntime::type(L, -1) == LUA_TNUMBER)
    {
        value = LuaRuntime::tonumber(L, -1);
    }
    LuaRuntime::settop(L, top);
    return value;
}

// The evaluator's faction arguments are the AI's wrappers (CAI_FACTION), not FACTION objects. The wrapper points to its
// FACTION; we find that field once by matching it against the negotiation's two factions, then cache the offset.
std::atomic<int64_t> caiFactionOffset = -1;

void* EvaluatingFaction(const EvalContext& eval)
{
    auto* negotiation = static_cast<uint8_t*>(eval.negotiation);
    void* factionA = *reinterpret_cast<void**>(negotiation + Addresses::Negotiation_FactionA);
    void* factionB = *reinterpret_cast<void**>(negotiation + Addresses::Negotiation_FactionB);
    auto* cai = static_cast<uint8_t*>(eval.factionA);
    if (!cai)
    {
        return nullptr;
    }
    if (cai == factionA || cai == factionB)
    {
        return cai;
    }
    int64_t offset = caiFactionOffset;
    if (offset < 0)
    {
        for (int64_t o = 0; o < 0x200; o += 8)
        {
            void* p = *reinterpret_cast<void**>(cai + o);
            if (p && (p == factionA || p == factionB))
            {
                offset = o;
                break;
            }
        }
        if (offset < 0)
        {
            spdlog::warn("[RegionTrading] can't find the FACTION of the evaluating AI");
            return nullptr;
        }
        caiFactionOffset = offset;
        spdlog::info("[RegionTrading] the evaluating AI's FACTION is at +{:#x}", offset);
    }
    return *reinterpret_cast<void**>(cai + offset);
}

// TRANSFER: the Lua handler around the engine's transfer (the mod protects governors)

void CallTransferHandler(const char* phase, const char* region, const char* faction)
{
    lua_State* L = luaState;
    if (!L || !region || !faction)
    {
        return;
    }
    const int top = LuaRuntime::gettop(L);
    LuaRuntime::getfield(L, LUA_REGISTRYINDEX, "twase_region_transfer_handler");
    if (LuaRuntime::type(L, -1) == LUA_TFUNCTION)
    {
        LuaRuntime::pushstring(L, phase);
        LuaRuntime::pushstring(L, region);
        LuaRuntime::pushstring(L, faction);
        if (LuaRuntime::pcall(L, 3, 0, 0) != 0)
        {
            spdlog::error("[RegionTrading] transfer handler failed: {}", LuaRuntime::tolstring(L, -1, nullptr));
        }
    }
    LuaRuntime::settop(L, top);
}

void Execute(void* action)
{
    auto* a = static_cast<uint8_t*>(action);
    auto* negotiation = a ? *reinterpret_cast<uint8_t**>(a + Addresses::DiplomaticAction_Negotiation) : nullptr;
    if (!isEnabled || !negotiation)
    {
        Execute_fnc(action);
        return;
    }
    // region and new owner keys, copied before the transfer
    std::vector<std::pair<std::string, std::string>> transfers;
    const struct { uint64_t countOffset; uint64_t receiverOffset; } lists[] = {
        {Addresses::TradeRegions_List1Count, Addresses::Negotiation_FactionA},
        {Addresses::TradeRegions_List2Count, Addresses::Negotiation_FactionB},
    };
    for (const auto& list : lists)
    {
        const auto count = *reinterpret_cast<uint32_t*>(a + list.countOffset);
        auto** regions = *reinterpret_cast<void***>(a + list.countOffset + 4);
        const char* receiver = KeyOf(*reinterpret_cast<void**>(negotiation + list.receiverOffset), Addresses::Faction_Record);
        for (uint32_t i = 0; regions && receiver && i < count; i++)
        {
            if (const char* region = KeyOf(regions[i], Addresses::Region_Record))
            {
                transfers.emplace_back(region, receiver);
            }
        }
    }
    for (const auto& [region, to] : transfers)
    {
        spdlog::debug("[RegionTrading] transfer {} to {}", region, to);
        CallTransferHandler("before", region.c_str(), to.c_str());
    }
    Execute_fnc(action);
    for (const auto& [region, to] : transfers)
    {
        CallTransferHandler("after", region.c_str(), to.c_str());
    }
}

// Gold value of the deal's regions for the evaluating faction: + for regions it receives, - for regions it gives
float RegionDealValue(const EvalContext& eval)
{
    auto* negotiation = static_cast<uint8_t*>(eval.negotiation);
    const auto count = *reinterpret_cast<uint32_t*>(negotiation + Addresses::Negotiation_ActionCount);
    auto** actions = *reinterpret_cast<uint8_t***>(negotiation + Addresses::Negotiation_Actions);
    uint8_t* action = nullptr;
    for (uint32_t i = 0; actions && i < count; i++)
    {
        if (actions[i] && *reinterpret_cast<int32_t*>(actions[i] + Addresses::DiplomaticAction_Type) == Addresses::DiplomaticAction_TradeRegions)
        {
            action = actions[i];
            break;
        }
    }
    if (!action)
    {
        return 0.0f;
    }
    const auto inDeal = *reinterpret_cast<InDeal_t*>(*reinterpret_cast<uint8_t**>(action) + Addresses::DiplomaticAction_InDealVtableOffset);
    if (!inDeal(action))
    {
        return 0.0f;
    }

    void* evaluator = EvaluatingFaction(eval);
    if (!evaluator)
    {
        return 0.0f;
    }
    const char* evaluatorKey = KeyOf(evaluator, Addresses::Faction_Record);
    float total = 0.0f;
    const struct { uint64_t countOffset; uint64_t receiverOffset; } lists[] = {
        {Addresses::TradeRegions_List1Count, Addresses::Negotiation_FactionA},
        {Addresses::TradeRegions_List2Count, Addresses::Negotiation_FactionB},
    };
    for (const auto& list : lists)
    {
        const auto regions = *reinterpret_cast<uint32_t*>(action + list.countOffset);
        auto** data = *reinterpret_cast<void***>(action + list.countOffset + 4);
        void* receiver = *reinterpret_cast<void**>(negotiation + list.receiverOffset);
        for (uint32_t i = 0; data && i < regions; i++)
        {
            const float value = HandlerValue(KeyOf(data[i], Addresses::Region_Record), evaluatorKey);
            total += receiver == evaluator ? value : -value;
            spdlog::debug("[RegionTrading] deal region {} for {}: {} ({})", KeyOf(data[i], Addresses::Region_Record),
                          evaluatorKey ? evaluatorKey : "?", value, receiver == evaluator ? "receives" : "gives");
        }
    }
    return total;
}

float EvaluateDeal(void* negotiation, void* aiContext, void* a, void* b, int32_t* out5, char* out6)
{
    EvalContext eval{negotiation, aiContext, a, b};
    EvalContext* previous = currentEval;
    currentEval = &eval;
    const float score = EvaluateDeal_fnc(negotiation, aiContext, a, b, out5, out6);
    currentEval = previous;
    return score;
}

float ScoreComponents(ComponentList* list, void* aiContext, void* a, int64_t* a4, int32_t* a5, char* a6, uint64_t a7)
{
    const EvalContext* eval = currentEval;
    const float gold = isEnabled && eval && eval->negotiation && list ? RegionDealValue(*eval) : 0.0f;
    if (gold == 0.0f)
    {
        return ScoreComponents_fnc(list, aiContext, a, a4, a5, a6, a7);
    }

    const float score = ScoreComponents_fnc(list, aiContext, a, a4, a5, a6, a7);

    // The evaluator only gives a gift a deal value when the AI pays it (a table lookup on the amount, relative to its
    // treasury); gold the AI receives only counts as goodwill. So a region worth V to the AI counts like the AI not having
    // to pay V: the mirror image of the score of "the AI pays V", computed by the same scorer. A burden (V < 0) counts
    // like paying |V|.
    static const auto giftCtor = reinterpret_cast<GiftCtor_t>(Addresses::DealComponentGift_ctor + Image::Get()->GetSlide());
    const auto payingScore = [&](float amount)
    {
        alignas(16) uint8_t gift[Addresses::DealComponentGift_Size] = {};
        giftCtor(gift, eval->aiContext, eval->factionA, eval->factionB, -static_cast<int32_t>(amount));
        ComponentEntry entry{gift, Addresses::DiplomaticAction_TradeRegions, 0};
        ComponentList single{1, 1, &entry};
        int32_t dummy5 = 0;
        char dummy6 = 0;
        return ScoreComponents_fnc(&single, aiContext, a, a4, &dummy5, &dummy6, a7);
    };
    // The table ignores amounts that are small for the AI's treasury (a swap 439 gold short scored 0 for the ERE).
    // Below its first step we interpolate linearly from the smallest doubled amount that counts.
    const float amount = std::fabs(gold);
    float paying = payingScore(amount);
    for (float reference = amount * 2; paying == 0.0f && reference < amount * 4096; reference *= 2)
    {
        if (const float referenceScore = payingScore(reference); referenceScore != 0.0f)
        {
            paying = referenceScore * amount / reference;
        }
    }
    const float delta = (gold > 0 ? -paying : paying) * static_cast<float>(valueSign.load());

    spdlog::debug("[RegionTrading] regions worth {} gold: deal score {} {:+} = {}", gold, score, delta, score + delta);
    return score + delta;
}
void* FindChild(void* parent, const char* name)
{
    const auto slide = Image::Get()->GetSlide();
    static const auto findChild = reinterpret_cast<FindChild_t>(Addresses::UIComponent_FindChild + slide);
    static const auto stringCtor = reinterpret_cast<StringCtor_t>(Addresses::String_ctor + slide);
    static const auto stringDtor = reinterpret_cast<StringDtor_t>(Addresses::String_dtor + slide);
    TempString key;
    stringCtor(&key, name);
    void* child = parent ? findChild(parent, &key, true) : nullptr;
    stringDtor(&key);
    return child;
}

// UIComponent geometry: position at + 0x128 / + 0x12c, size in the current state (+ 0x140) at + 0x40 / + 0x44,
// MoveTo is virtual + 0xc8 with {x, y}, all floats (the Lua bindings Position, Dimensions and MoveTo)
std::pair<float, float> Position(void* component)
{
    auto* c = static_cast<uint8_t*>(component);
    return {*reinterpret_cast<float*>(c + 0x128), *reinterpret_cast<float*>(c + 0x12c)};
}

float Width(void* component)
{
    auto* state = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(component) + 0x140);
    return state ? *reinterpret_cast<float*>(state + 0x40) : 0.0f;
}

void MoveTo(void* component, float x, float y)
{
    float position[2] = {x, y};
    auto moveTo = *reinterpret_cast<void (**)(void*, float*)>(*reinterpret_cast<uint8_t**>(component) + 0xc8);
    moveTo(component, position);
}

float Height(void* component)
{
    auto* state = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(component) + 0x140);
    return state ? *reinterpret_cast<float*>(state + 0x44) : 0.0f;
}

// virtual + 0xa8 (the Lua Resize); a component only changes the width / height it may resize (flags + 0x190 / + 0x191),
// so we allow both first
void Resize(void* component, float width, float height)
{
    static_cast<uint8_t*>(component)[0x190] = 1;
    static_cast<uint8_t*>(component)[0x191] = 1;
    auto resize = *reinterpret_cast<void (**)(void*, bool, bool, float, float)>(*reinterpret_cast<uint8_t**>(component) + 0xa8);
    resize(component, true, true, width, height);
}

// MULTIPLE SELECTION

std::vector<void*> selectedRows; // ticked rows of both lists, the last one is the engine's selection

void* Slid(uint64_t address)
{
    return reinterpret_cast<void*>(address + Image::Get()->GetSlide());
}

void* LocalFaction()
{
    void* campaign = *static_cast<void**>(Slid(Addresses::g_Campaign));
    return campaign ? reinterpret_cast<void* (*)(void*)>(Slid(Addresses::Campaign_LocalFaction))(campaign) : nullptr;
}

void* OtherFaction(void* panel)
{
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(panel) + Addresses::TradeRegionsPanel_OtherFaction);
}

void* Root(void* panel)
{
    return panel ? *reinterpret_cast<void**>(static_cast<uint8_t*>(panel) + Addresses::TradeRegionsPanel_Component) : nullptr;
}

bool IsWidenedPanel(void* panel)
{
    return isEnabled && panel && IsSettledPair(LocalFaction(), OtherFaction(panel));
}

// before Populate has set the panel's faction
bool IsWidenedAction(void* action)
{
    if (!isEnabled || !action)
    {
        return false;
    }
    using Faction_t = void* (*)(void*);
    void* local = LocalFaction();
    void* other = reinterpret_cast<Faction_t>(Slid(Addresses::DiplomaticAction_FactionA))(action);
    if (other == local)
    {
        other = reinterpret_cast<Faction_t>(Slid(Addresses::DiplomaticAction_FactionB))(action);
    }
    return IsSettledPair(local, other);
}

std::span<void*> Children(void* component)
{
    if (!component)
    {
        return {};
    }
    auto* c = static_cast<uint8_t*>(component);
    return {*reinterpret_cast<void***>(c + Addresses::UIComponent_Children), *reinterpret_cast<uint32_t*>(c + Addresses::UIComponent_ChildCount)};
}

// the rows of list_offers / list_demands
std::span<void*> Rows(void* root, const char* list)
{
    return Children(FindChild(FindChild(root, list), "list_box"));
}

int32_t RowRegionId(void* row)
{
    int32_t id = 0;
    reinterpret_cast<bool (*)(void*, int32_t*)>(Slid(Addresses::TempString_ToInt))(static_cast<uint8_t*>(row) + Addresses::UIComponent_Id, &id);
    return id;
}

void SetChecked(void* row, bool checked)
{
    void* checkbox = FindChild(row, "checkbox");
    void* state = checkbox ? *reinterpret_cast<void**>(static_cast<uint8_t*>(checkbox) + Addresses::UIComponent_Checkbox) : nullptr;
    if (state)
    {
        reinterpret_cast<void (*)(void*, bool, bool, bool)>(Slid(Addresses::Checkbox_SetChecked))(state, checked, false, false);
    }
}

bool HasId(void* component, const char* id)
{
    const auto* key = reinterpret_cast<const TempString*>(static_cast<uint8_t*>(component) + Addresses::UIComponent_Id);
    return key->length > 0 && std::strcmp(key->data, id) == 0;
}

// A click on a list row: the engine selects the row when its checkbox was ticked and nothing when it was unticked (one
// selection). We keep every ticked row of both lists; the last one stays the engine's selection (for the OK button's
// state and the camera).
uint64_t OnClick(void* panel, void** event)
{
    void* component = event ? event[0] : nullptr;
    if (!component || !IsWidenedPanel(panel) || HasId(component, "ok_trade_regions") || HasId(component, "cancel_trade_regions"))
    {
        return OnClick_fnc(panel, event);
    }
    static const auto findChild = reinterpret_cast<FindChild_t>(Slid(Addresses::UIComponent_FindChild));
    static const auto stringCtor = reinterpret_cast<StringCtor_t>(Slid(Addresses::String_ctor));
    static const auto stringDtor = reinterpret_cast<StringDtor_t>(Slid(Addresses::String_dtor));
    TempString key;
    stringCtor(&key, "list_box");
    void* box = findChild(component, &key, false);
    stringDtor(&key);
    void* row = box ? *reinterpret_cast<void**>(static_cast<uint8_t*>(component) + Addresses::UIComponent_ClickedRow) : nullptr;
    if (!row)
    {
        return OnClick_fnc(panel, event);
    }

    const bool ticked = reinterpret_cast<intptr_t>(event[1]) == 1;
    auto it = std::find(selectedRows.begin(), selectedRows.end(), row);
    if (ticked && it == selectedRows.end())
    {
        selectedRows.push_back(row);
    }
    else if (!ticked && it != selectedRows.end())
    {
        selectedRows.erase(it);
    }
    static const auto selectRow = reinterpret_cast<SelectRow_t>(Slid(Addresses::TradeRegionsPanel_SelectRow));
    selectRow(panel, ticked ? row : (selectedRows.empty() ? nullptr : selectedRows.back()));
    for (void* selected : selectedRows)
    {
        SetChecked(selected, true);
    }
    return 0;
}

struct PointerList
{
    uint32_t capacity;
    uint32_t count;
    void** data;
};

// regions of `faction` by id, from the same list the panel shows
std::vector<void*> RegionsById(void* faction, const std::vector<int32_t>& ids)
{
    std::vector<void*> regions;
    if (ids.empty())
    {
        return regions;
    }
    RegionList list = GiftableRegions_fnc(faction, nullptr);
    for (int32_t id : ids)
    {
        for (uint32_t i = 0; i < list.count; i++)
        {
            auto* region = static_cast<uint8_t**>(list.data)[i];
            if (*reinterpret_cast<int32_t*>(region + Addresses::Region_Id) == id)
            {
                regions.push_back(region);
                break;
            }
        }
    }
    if (list.data)
    {
        reinterpret_cast<void (*)(void*)>(Slid(Addresses::tw_free))(list.data);
    }
    return regions;
}

void* openPanel = nullptr;

// OK: all ticked regions go into the deal, offered and demanded
void SetRegion(void* context, int32_t regionId, void* demandedFrom, void* deal)
{
    void* panel = openPanel;
    if (selectedRows.empty() || !deal || !IsWidenedPanel(panel))
    {
        SetRegion_fnc(context, regionId, demandedFrom, deal);
        return;
    }
    const auto demandRows = Rows(Root(panel), "list_demands");
    std::vector<int32_t> offeredIds, demandedIds;
    for (void* row : selectedRows)
    {
        const bool demanded = std::find(demandRows.begin(), demandRows.end(), row) != demandRows.end();
        (demanded ? demandedIds : offeredIds).push_back(RowRegionId(row));
    }
    selectedRows.clear();
    std::vector<void*> offered = RegionsById(LocalFaction(), offeredIds);
    std::vector<void*> demanded = RegionsById(OtherFaction(panel), demandedIds);
    spdlog::debug("[RegionTrading] deal regions: {} offered, {} demanded", offered.size(), demanded.size());

    auto* c = static_cast<uint8_t*>(context);
    void* x = *reinterpret_cast<void**>(*reinterpret_cast<uint8_t**>(*reinterpret_cast<uint8_t**>(c + 0xc0) + 0x30) + 0x50);
    alignas(16) uint8_t terms[0x200];
    reinterpret_cast<void (*)(void*)>(Slid(Addresses::DealTerms_ctor))(terms);
    PointerList offeredList{static_cast<uint32_t>(offered.size()), static_cast<uint32_t>(offered.size()), offered.data()};
    PointerList demandedList{static_cast<uint32_t>(demanded.size()), static_cast<uint32_t>(demanded.size()), demanded.data()};
    reinterpret_cast<void (*)(void*, const PointerList*)>(Slid(Addresses::DealTerms_SetOfferedRegions))(terms, &offeredList);
    reinterpret_cast<void (*)(void*, const PointerList*)>(Slid(Addresses::DealTerms_SetDemandedRegions))(terms, &demandedList);
    reinterpret_cast<void (*)(void*, void*, void*)>(Slid(Addresses::Deal_ApplyTerms))(deal, x, terms);
    reinterpret_cast<void (*)(void*)>(Slid(Addresses::DealTerms_dtor))(terms);
}

// DEAL LIST: a trade regions row shows the demanded regions if there are any, else the offered ones, in both columns.
// With both (a swap) we show each column its own: in "your_offers" the regions you own, in the other the AI's.
void ListButtonSetup(void* button, void* root, void* entry, void* context)
{
    ListButtonSetup_fnc(button, root, entry, context);
    auto* action = entry ? *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(entry) + 0x18) : nullptr;
    if (!isEnabled || !action || !button || !root)
    {
        return;
    }
    const auto count1 = *reinterpret_cast<uint32_t*>(action + Addresses::TradeRegions_List1Count);
    const auto count2 = *reinterpret_cast<uint32_t*>(action + Addresses::TradeRegions_List2Count);
    void* row = *reinterpret_cast<void**>(static_cast<uint8_t*>(button) + 8);
    void* names = row ? FindChild(row, "region_names") : nullptr;
    if (count1 == 0 || count2 == 0 || !names)
    {
        return;
    }
    const auto yourRows = Rows(root, "your_offers");
    const bool yours = std::find(yourRows.begin(), yourRows.end(), row) != yourRows.end();
    void* local = LocalFaction();

    using UniCtor_t = void (*)(void*, const char16_t*);
    using UniAppend_t = void (*)(void*, const void*);
    using UniAppendLiteral_t = void (*)(void*, const char16_t*);
    using UniNotEmpty_t = bool (*)(const void*);
    using UniDtor_t = void (*)(void*);
    alignas(16) uint8_t text[16];
    reinterpret_cast<UniCtor_t>(Slid(Addresses::UniString_ctor))(text, u"");
    for (uint64_t countOffset : {Addresses::TradeRegions_List1Count, Addresses::TradeRegions_List2Count})
    {
        const auto count = *reinterpret_cast<uint32_t*>(action + countOffset);
        auto** regions = *reinterpret_cast<uint8_t***>(action + countOffset + 4);
        for (uint32_t i = 0; regions && i < count; i++)
        {
            auto** ownerSlot = *reinterpret_cast<void***>(regions[i] + Addresses::Region_Owner);
            const bool owned = ownerSlot && *ownerSlot == local;
            if (owned != yours)
            {
                continue;
            }
            if (reinterpret_cast<UniNotEmpty_t>(Slid(Addresses::UniString_NotEmpty))(text))
            {
                reinterpret_cast<UniAppendLiteral_t>(Slid(Addresses::UniString_AppendLiteral))(text, u", ");
            }
            const void* name = reinterpret_cast<const void* (*)(void*)>(Slid(Addresses::Region_LocalisedName))(regions[i]);
            reinterpret_cast<UniAppend_t>(Slid(Addresses::UniString_Append))(text, name);
        }
    }
    reinterpret_cast<void (*)(void*, const void*, bool)>(Slid(Addresses::UIComponent_SetText))(names, text, false);
    reinterpret_cast<UniDtor_t>(Slid(Addresses::UniString_dtor))(text);
}

// The panel only shows the offers ("Gift Region"); show the demands list too, side by side, and tick every region
// already in the deal
void* PopulatePanel(void* panel, void* action)
{
    void* root = Root(panel);
    const bool widened = IsWidenedAction(action);
    if (widened && root)
    {
        // before populating: the engine only fills visible lists, and the layout hides the demands
        static const auto setVisible = reinterpret_cast<SetVisible_t>(Slid(Addresses::UIComponent_SetVisible));
        for (const char* name : {"list_demands", "demands_title"})
        {
            if (void* component = FindChild(root, name))
            {
                setVisible(component, true);
            }
        }
    }
    selectedRows.clear();
    openPanel = panel;
    void* result = PopulatePanel_fnc(panel, action);
    if (!widened || !root)
    {
        return result;
    }

    // the panel fits one list: narrow both lists to fit side by side
    void* offers = FindChild(root, "list_offers");
    void* demands = FindChild(root, "list_demands");
    if (offers && demands)
    {
        constexpr float gap = 16.0f;
        const auto [rootX, rootY] = Position(root);
        const float rootWidth = Width(root);
        const float listWidth = (rootWidth - 3 * gap) / 2;
        const float y = Position(offers).second;
        for (void* list : {offers, demands})
        {
            // the list's clip and box follow its width, the rows (and their checkboxes on the right) don't
            Resize(list, listWidth, Height(list));
            void* box = FindChild(list, "list_box");
            const float rowWidth = box ? Width(box) : 0.0f;
            for (void* row : Children(box))
            {
                Resize(row, rowWidth, Height(row));
                if (void* checkbox = FindChild(row, "checkbox"))
                {
                    MoveTo(checkbox, Position(row).first + rowWidth - Width(checkbox) - 8, Position(checkbox).second);
                }
            }
        }
        MoveTo(offers, rootX + gap, y);
        MoveTo(demands, rootX + 2 * gap + listWidth, y);
        spdlog::debug("[RegionTrading] region panel: x {} width {}, lists width {}", rootX, rootWidth, Width(offers));
    }

    // tick every region already in the deal (the engine ticks only the first one)
    auto* a = static_cast<uint8_t*>(action);
    const auto inDeal = a ? *reinterpret_cast<InDeal_t*>(*reinterpret_cast<uint8_t**>(a) + Addresses::DiplomaticAction_InDealVtableOffset) : nullptr;
    if (inDeal && inDeal(action))
    {
        for (uint64_t countOffset : {Addresses::TradeRegions_List1Count, Addresses::TradeRegions_List2Count})
        {
            const auto count = *reinterpret_cast<uint32_t*>(a + countOffset);
            auto** regions = *reinterpret_cast<uint8_t***>(a + countOffset + 4);
            for (uint32_t i = 0; regions && i < count; i++)
            {
                const int32_t id = *reinterpret_cast<int32_t*>(regions[i] + Addresses::Region_Id);
                for (const char* list : {"list_offers", "list_demands"})
                {
                    for (void* row : Rows(root, list))
                    {
                        if (RowRegionId(row) == id && std::find(selectedRows.begin(), selectedRows.end(), row) == selectedRows.end())
                        {
                            selectedRows.push_back(row);
                            SetChecked(row, true);
                        }
                    }
                }
            }
        }
    }
    return result;
}
} // namespace

void Hooks::NativeRegionTradingHook::SetLuaState(lua_State* L)
{
    luaState = L;
}

void Hooks::NativeRegionTradingHook::SetValueSign(int aSign)
{
    valueSign = aSign < 0 ? -1 : 1;
}

bool Hooks::NativeRegionTradingHook::IsEnabled()
{
    return isEnabled;
}

void Hooks::NativeRegionTradingHook::SetEnabled(bool aEnabled)
{
    isEnabled = aEnabled;
    spdlog::info("Native region trading {}", aEnabled ? "enabled" : "disabled");
}

bool Hooks::NativeRegionTradingHook::Attach()
{
    auto result = GiftableRegions_fnc.Attach();
    if (result == 0)
        result = RegionTradePossible_fnc.Attach();
    if (result == 0)
        result = PopulatePanel_fnc.Attach();
    if (result == 0)
        result = OnClick_fnc.Attach();
    if (result == 0)
        result = SetRegion_fnc.Attach();
    if (result == 0)
        result = Execute_fnc.Attach();
    if (result == 0)
        result = ListButtonSetup_fnc.Attach();
    if (result == 0)
        result = EvaluateDeal_fnc.Attach();
    if (result == 0)
        result = ScoreComponents_fnc.Attach();
    if (result != 0)
    {
        spdlog::error("Could not attach the native region trading hooks. Dobby error code: {}", result);
    }
    else
    {
        spdlog::info("The native region trading hooks were attached");
    }
    isAttached = result == 0;
    return isAttached;
}

bool Hooks::NativeRegionTradingHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }
    auto result = ScoreComponents_fnc.Detach();
    if (result == 0)
        result = EvaluateDeal_fnc.Detach();
    if (result == 0)
        result = ListButtonSetup_fnc.Detach();
    if (result == 0)
        result = Execute_fnc.Detach();
    if (result == 0)
        result = SetRegion_fnc.Detach();
    if (result == 0)
        result = OnClick_fnc.Detach();
    if (result == 0)
        result = PopulatePanel_fnc.Detach();
    if (result == 0)
        result = RegionTradePossible_fnc.Detach();
    if (result == 0)
        result = GiftableRegions_fnc.Detach();
    isAttached = result != 0;
    return !isAttached;
}

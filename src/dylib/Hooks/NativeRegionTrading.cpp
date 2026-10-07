#include "stdafx.hpp"

#include "NativeRegionTrading.hpp"

#include "../Image.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/VFSTypes.hpp"

#include "../../sdk/Attila/Addresses.hpp"
#include "../Scripting/LuaApi.hpp"

// Attila's diplomacy has a "Gift Region" option (diplomatic action type 6, permission "regions"), but only to settle a
// horde: the action is available when one side is a horde, and only regions where the horde's army stands can be given.
// With region trading enabled we widen both rules for two settled factions:
// - available: also between a human and a non-horde AI faction, if the human has a region to give
// - regions: between a human and a non-horde, either side can give any region but its capital and garrisoned
//   settlements (offers and demands; the engine only lists regions where the receiver's army stands)
// The panel only lets you tick one region (in either list); we let you tick any number in both, so one deal can give
// and demand regions (a swap), and OK puts them all into the deal.
// "Region for troops" (federates): a second button for the region action in the "Add Offer/Demand" list opens the same
// panel in troops mode: your regions on the left, the mod's offer of their warriors on the right.
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
int64_t MakeActionButtons(void* dropdown);
Hook<decltype(&MakeActionButtons)> MakeActionButtons_fnc(Addresses::DiplomacyDropdown_MakeActionButtons, &MakeActionButtons);
void OnActionButton(void* dropdown, void* event);
Hook<decltype(&OnActionButton)> OnActionButton_fnc(Addresses::DiplomacyDropdown_OnActionButton, &OnActionButton);
void OnActionButtonThunk(void* dropdown, void* event);
Hook<decltype(&OnActionButtonThunk)> OnActionButtonThunk_fnc(Addresses::DiplomacyDropdown_OnActionButtonThunk, &OnActionButtonThunk);

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

// The Lua handler's gold value of `region` for `faction` in a deal with `other` (0 without a handler)
float HandlerValue(const char* region, const char* faction, const char* other)
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
    if (other)
    {
        LuaRuntime::pushstring(L, other);
    }
    else
    {
        LuaRuntime::pushnil(L);
    }
    float value = 0.0f;
    if (LuaRuntime::pcall(L, 3, 1, 0) != 0)
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

// the negotiation's action of a type (the actions aren't indexed by type), nullptr if it has none
uint8_t* FindAction(void* negotiation, int type, int32_t* index = nullptr)
{
    auto* n = static_cast<uint8_t*>(negotiation);
    const auto count = *reinterpret_cast<uint32_t*>(n + Addresses::Negotiation_ActionCount);
    auto** actions = *reinterpret_cast<uint8_t***>(n + Addresses::Negotiation_Actions);
    for (uint32_t i = 0; actions && i < count; i++)
    {
        if (actions[i] && *reinterpret_cast<int32_t*>(actions[i] + Addresses::DiplomaticAction_Type) == type)
        {
            if (index)
            {
                *index = static_cast<int32_t>(i);
            }
            return actions[i];
        }
    }
    return nullptr;
}

// REGION FOR TROOPS state: the negotiation whose region term is "for troops"
void* troopsNegotiation = nullptr;

// The mod's willingness of `ai` to become `player`'s federates, in the game's score points (0 without a handler)
float HandlerWillingness(const char* ai, const char* player)
{
    lua_State* L = luaState;
    if (!L || !ai || !player)
    {
        return 0.0f;
    }
    const int top = LuaRuntime::gettop(L);
    LuaRuntime::getfield(L, LUA_REGISTRYINDEX, "twase_region_willingness_handler");
    float value = 0.0f;
    if (LuaRuntime::type(L, -1) == LUA_TFUNCTION)
    {
        LuaRuntime::pushstring(L, ai);
        LuaRuntime::pushstring(L, player);
        if (LuaRuntime::pcall(L, 2, 1, 0) != 0)
        {
            spdlog::error("[RegionTrading] willingness handler failed: {}", LuaRuntime::tolstring(L, -1, nullptr));
        }
        else if (LuaRuntime::type(L, -1) == LUA_TNUMBER)
        {
            value = LuaRuntime::tonumber(L, -1);
        }
    }
    LuaRuntime::settop(L, top);
    return value;
}

// The mod's foedus handler around the transfer of a deal for troops: function(phase, ai_key, player_key, region_key...)
void CallFoedusHandler(const char* phase, const char* ai, const char* player, const std::vector<std::string>& regions)
{
    lua_State* L = luaState;
    if (!L || !ai || !player)
    {
        return;
    }
    const int top = LuaRuntime::gettop(L);
    LuaRuntime::getfield(L, LUA_REGISTRYINDEX, "twase_region_foedus_handler");
    if (LuaRuntime::type(L, -1) == LUA_TFUNCTION)
    {
        LuaRuntime::pushstring(L, phase);
        LuaRuntime::pushstring(L, ai);
        LuaRuntime::pushstring(L, player);
        for (const auto& region : regions)
        {
            LuaRuntime::pushstring(L, region.c_str());
        }
        if (LuaRuntime::pcall(L, 3 + static_cast<int>(regions.size()), 0, 0) != 0)
        {
            spdlog::error("[RegionTrading] foedus handler failed: {}", LuaRuntime::tolstring(L, -1, nullptr));
        }
    }
    LuaRuntime::settop(L, top);
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
    // a deal for troops: the regions go to the AI, the mod adds its warriors to your army around the transfer
    const bool troops = negotiation == troopsNegotiation;
    void* factionA = *reinterpret_cast<void**>(negotiation + Addresses::Negotiation_FactionA);
    void* factionB = *reinterpret_cast<void**>(negotiation + Addresses::Negotiation_FactionB);
    const char* player = KeyOf(IsHuman(factionA) ? factionA : factionB, Addresses::Faction_Record);
    const char* ai = KeyOf(IsHuman(factionA) ? factionB : factionA, Addresses::Faction_Record);
    std::vector<std::string> ceded;
    for (const auto& [region, to] : transfers)
    {
        ceded.push_back(region);
    }
    if (troops)
    {
        CallFoedusHandler("before", ai, player, ceded);
    }
    for (const auto& [region, to] : transfers)
    {
        spdlog::debug("[RegionTrading] transfer {} to {}{}", region, to, troops ? " for troops" : "");
        CallTransferHandler("before", region.c_str(), to.c_str());
    }
    Execute_fnc(action);
    for (const auto& [region, to] : transfers)
    {
        CallTransferHandler("after", region.c_str(), to.c_str());
    }
    if (troops)
    {
        CallFoedusHandler("after", ai, player, ceded);
        troopsNegotiation = nullptr;
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
    void* factionA = *reinterpret_cast<void**>(negotiation + Addresses::Negotiation_FactionA);
    void* factionB = *reinterpret_cast<void**>(negotiation + Addresses::Negotiation_FactionB);
    const char* otherKey = KeyOf(evaluator == factionA ? factionB : factionA, Addresses::Faction_Record);
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
            const float value = HandlerValue(KeyOf(data[i], Addresses::Region_Record), evaluatorKey, otherKey);
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
    bool troops = isEnabled && eval && eval->negotiation && list && eval->negotiation == troopsNegotiation;
    if (troops)
    {
        // only while the region term is still in the deal (it can be removed from the deal list)
        uint8_t* regions = FindAction(eval->negotiation, Addresses::DiplomaticAction_TradeRegions);
        troops = regions && (*reinterpret_cast<InDeal_t*>(*reinterpret_cast<uint8_t**>(regions) + Addresses::DiplomaticAction_InDealVtableOffset))(regions);
    }
    const float gold = isEnabled && eval && eval->negotiation && list ? RegionDealValue(*eval) : 0.0f;
    if (gold == 0.0f && !troops)
    {
        return ScoreComponents_fnc(list, aiContext, a, a4, a5, a6, a7);
    }

    // A deal for troops (a foedus): the native peace and tributary terms that come with it aren't scored by the game;
    // instead the mod's willingness of the AI to become your federates counts (the components' index is their action's
    // position in the negotiation's action array)
    ComponentList* scored = list;
    std::vector<ComponentEntry> kept;
    ComponentList filtered{};
    float willingness = 0.0f;
    if (troops)
    {
        auto* n = static_cast<uint8_t*>(eval->negotiation);
        const auto count = *reinterpret_cast<uint32_t*>(n + Addresses::Negotiation_ActionCount);
        auto** actions = *reinterpret_cast<uint8_t***>(n + Addresses::Negotiation_Actions);
        for (uint32_t i = 0; i < list->count; i++)
        {
            const int32_t index = list->data[i].index;
            const int32_t type = index >= 0 && static_cast<uint32_t>(index) < count && actions[index]
                                     ? *reinterpret_cast<int32_t*>(actions[index] + Addresses::DiplomaticAction_Type)
                                     : -1;
            if (type != Addresses::DiplomaticAction_Peace && type != Addresses::DiplomaticAction_MakeTributary)
            {
                kept.push_back(list->data[i]);
            }
        }
        filtered = {static_cast<uint32_t>(kept.size()), static_cast<uint32_t>(kept.size()), kept.data()};
        scored = &filtered;
        void* evaluator = EvaluatingFaction(*eval);
        void* factionA = *reinterpret_cast<void**>(n + Addresses::Negotiation_FactionA);
        void* factionB = *reinterpret_cast<void**>(n + Addresses::Negotiation_FactionB);
        willingness = HandlerWillingness(KeyOf(evaluator, Addresses::Faction_Record),
                                         KeyOf(evaluator == factionA ? factionB : factionA, Addresses::Faction_Record));
        spdlog::debug("[RegionTrading] deal for troops: {} of {} components scored natively, willingness {:+}", kept.size(),
                      list->count, willingness);
    }

    const float score = ScoreComponents_fnc(scored, aiContext, a, a4, a5, a6, a7) + willingness;
    if (gold == 0.0f)
    {
        return score;
    }

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
    // Part of the score sizes the gift (small / medium / large from 500 / 2,000 / 7,500 x an economic power) against the
    // other side of the deal (the gift's second faction, you): clamp(GDP / 8,000 + treasury / 10,000, 0.2, 4) (campaign
    // variables economic_power_*). The treasury part moves within a turn, so for regions only the GDP counts: we scale
    // the amount by the power with the treasury over the power without it. (The main, linear part divides the amount by
    // the paying AI's GDP, which doesn't move within a turn.)
    float amount = std::fabs(gold);
    using EconomicPower_t = float (*)(void* caiFaction, int32_t treasuryOffset);
    using Treasury_t = int32_t (*)(void* treasury);
    static const auto economicPower = reinterpret_cast<EconomicPower_t>(Addresses::CaiFaction_EconomicPower + Image::Get()->GetSlide());
    static const auto treasuryOf = reinterpret_cast<Treasury_t>(Addresses::Treasury_Get + Image::Get()->GetSlide());
    auto* payer = static_cast<uint8_t*>(eval->factionB);
    auto* payerFaction = payer ? *reinterpret_cast<uint8_t**>(payer + Addresses::CaiFaction_Faction) : nullptr;
    if (payerFaction)
    {
        const int32_t treasury = treasuryOf(payerFaction + Addresses::Faction_Treasury);
        const float withTreasury = economicPower(payer, 0);
        const float gdpOnly = economicPower(payer, -treasury);
        if (gdpOnly > 0.0f)
        {
            amount *= withTreasury / gdpOnly;
        }
        spdlog::debug("[RegionTrading] gift size measured against {} (treasury {}): economic power {} with, {} without the treasury",
                      KeyOf(payerFaction, Addresses::Faction_Record) ? KeyOf(payerFaction, Addresses::Faction_Record) : "?",
                      treasury, withTreasury, gdpOnly);
    }

    // The table ignores amounts that are small for the payer (a swap 439 gold short scored 0 for the ERE).
    // Below its first step we interpolate linearly from the smallest doubled amount that counts.
    float paying = payingScore(amount);
    float interpolatedFrom = 0.0f;
    for (float reference = amount * 2; paying == 0.0f && reference < amount * 4096; reference *= 2)
    {
        if (const float referenceScore = payingScore(reference); referenceScore != 0.0f)
        {
            paying = referenceScore * amount / reference;
            interpolatedFrom = reference;
        }
    }
    const float delta = (gold > 0 ? -paying : paying);

    if (spdlog::should_log(spdlog::level::debug))
    {
        // The parts the scorer sums for one gift (vtable + 0x10 … + 0x30, see docs/addresses.md); + 0x18 needs the
        // scorer's context, so it's the rest. The amount part is linear: the amount over the paying AI's GDP, weighted by
        // its personality (more beyond its budget), capped.
        const float shown = interpolatedFrom > 0.0f ? interpolatedFrom : amount;
        alignas(16) uint8_t gift[Addresses::DealComponentGift_Size] = {};
        giftCtor(gift, eval->aiContext, eval->factionA, eval->factionB, -static_cast<int32_t>(shown));
        const auto slot = [&](uint64_t offset)
        {
            return (*reinterpret_cast<float (**)(void*)>(*reinterpret_cast<uint8_t**>(gift) + offset))(gift);
        };
        const float base = slot(0x10), size = slot(0x20), amountPart = slot(0x28), other = slot(0x30);
        const float total = payingScore(shown);
        static const auto gdpOf = reinterpret_cast<float (*)(void*)>(Addresses::Faction_Gdp + Image::Get()->GetSlide());
        void* evaluator = EvaluatingFaction(*eval);
        spdlog::debug("[RegionTrading] the game's score of {} paying {:.0f} gold: amount part {:.2f} (its GDP {:.0f}) + gift size "
                      "{:.2f} + table {:.2f} + base {:.2f} + other {:.2f} = {:.2f}{}",
                      KeyOf(evaluator, Addresses::Faction_Record) ? KeyOf(evaluator, Addresses::Faction_Record) : "?", shown,
                      amountPart, evaluator ? gdpOf(evaluator) : 0.0f, size, total - amountPart - size - base - other, base,
                      other, total,
                      interpolatedFrom > 0.0f ? fmt::format(" (below the first step: x {:.0f} / {:.0f} = {:.2f})", amount, shown, paying) : "");
    }
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

// REGION FOR TROOPS state: the open panel's mode, the negotiation whose region term is "for troops"
bool troopsMode = false;
int pendingMode = -1; // set by a click on an action button: 1 troops, 0 gift; -1 none (the panel was opened otherwise)
void UpdateTroopsList(void* panel);

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

    // a bool in the low byte, the rest of the word is undefined
    const bool ticked = *reinterpret_cast<const uint8_t*>(&event[1]) == 1;
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
    if (troopsMode)
    {
        UpdateTroopsList(panel);
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
    // for troops you only give land; the deal remembers it (its region term is "for troops")
    void* negotiation = *reinterpret_cast<void**>(static_cast<uint8_t*>(context) + Addresses::DiplomacyContext_Negotiation);
    if (troopsMode)
    {
        demandedIds.clear();
        troopsNegotiation = negotiation;
    }
    else if (troopsNegotiation == negotiation)
    {
        troopsNegotiation = nullptr;
    }
    std::vector<void*> offered = RegionsById(LocalFaction(), offeredIds);
    std::vector<void*> demanded = RegionsById(OtherFaction(panel), demandedIds);
    spdlog::debug("[RegionTrading] deal regions: {} offered, {} demanded{}", offered.size(), demanded.size(), troopsMode ? ", for troops" : "");

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

    // a foedus comes with peace (at war) and the tributary status: the native terms, where available. At war the
    // engine offers the tributary status once peace is in the deal, but updates the flag later, so after peace we
    // only check that it isn't blocked.
    if (troopsMode && negotiation)
    {
        bool peace = false;
        for (int type : {Addresses::DiplomaticAction_Peace, Addresses::DiplomaticAction_MakeTributary})
        {
            uint8_t* term = FindAction(negotiation, type);
            const bool blocked = term && term[Addresses::DiplomaticAction_Available + 1] != 0;
            const bool available = term && !blocked && (term[Addresses::DiplomaticAction_Available] == 1 || (type == Addresses::DiplomaticAction_MakeTributary && peace));
            if (available)
            {
                reinterpret_cast<void (*)(void*, void*)>(Slid(Addresses::DiplomacyContext_AddTerm))(context, term);
                peace = peace || type == Addresses::DiplomaticAction_Peace;
                spdlog::debug("[RegionTrading] deal for troops: added the native term {}", type);
            }
        }
    }
}

// REGION FOR TROOPS

std::u16string Utf16(const std::string& aUtf8)
{
    std::u16string out;
    for (size_t i = 0; i < aUtf8.size();)
    {
        const auto c = static_cast<uint8_t>(aUtf8[i]);
        const int length = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : 4;
        uint32_t code = length == 1 ? c : length == 2 ? c & 0x1f : length == 3 ? c & 0x0f : c & 0x07;
        for (int j = 1; j < length && i + j < aUtf8.size(); j++)
        {
            code = (code << 6) | (static_cast<uint8_t>(aUtf8[i + j]) & 0x3f);
        }
        if (code >= 0x10000)
        {
            code -= 0x10000;
            out += static_cast<char16_t>(0xd800 + (code >> 10));
            out += static_cast<char16_t>(0xdc00 + (code & 0x3ff));
        }
        else
        {
            out += static_cast<char16_t>(code);
        }
        i += length;
    }
    return out;
}

// a component's text or tooltip (the engine's UniString)
void SetText(void* component, const std::string& aText, bool aTooltip)
{
    const std::u16string text = Utf16(aText);
    alignas(16) uint8_t uni[16];
    reinterpret_cast<void (*)(void*, const char16_t*)>(Slid(Addresses::UniString_ctor))(uni, text.c_str());
    reinterpret_cast<void (*)(void*, const void*, bool)>(
        Slid(aTooltip ? Addresses::UIComponent_SetTooltipText : Addresses::UIComponent_SetText))(component, uni, aTooltip);
    reinterpret_cast<void (*)(void*)>(Slid(Addresses::UniString_dtor))(uni);
}

void* troopsButton = nullptr; // the "Region for Troops" button in the open "Add Offer/Demand" list

// an "Add Offer/Demand" list entry (DiplomacyDropdown + 0x1c0)
struct ActionEntry
{
    uint8_t text[16]; // UniString
    uint8_t flags[8];
    uint8_t* action;
};
static_assert(sizeof(ActionEntry) == 0x20);

// The list gets a second button for the region action, "Region for Troops": we add a copy of the region action's
// entry before the engine makes the buttons, then take the second button it made for it and remove the copy again.
int64_t MakeActionButtons(void* dropdown)
{
    troopsButton = nullptr;
    auto* vector = static_cast<uint8_t*>(dropdown) + Addresses::DiplomacyDropdown_ActionEntries;
    const auto count = [&] { return reinterpret_cast<uint32_t*>(vector + 4); };
    const auto entries = [&] { return *reinterpret_cast<ActionEntry**>(vector + 8); };

    int regionEntry = -1;
    for (uint32_t i = 0; isEnabled && i < *count(); i++)
    {
        uint8_t* action = entries()[i].action;
        if (action && *reinterpret_cast<int32_t*>(action + Addresses::DiplomaticAction_Type) == Addresses::DiplomaticAction_TradeRegions &&
            IsWidenedAction(action))
        {
            regionEntry = static_cast<int>(i);
        }
    }
    if (regionEntry < 0)
    {
        return MakeActionButtons_fnc(dropdown);
    }

    // At war, land is only traded in a peace: the region entries appear once peace is in the deal (like vanilla's
    // tributary). Peace is only available at war. The region entry is taken out of the list for the engine and put back.
    void* context = *reinterpret_cast<void**>(static_cast<uint8_t*>(dropdown) + Addresses::DiplomacyDropdown_Context);
    void* negotiation = context ? *reinterpret_cast<void**>(static_cast<uint8_t*>(context) + Addresses::DiplomacyContext_Negotiation) : nullptr;
    uint8_t* peace = negotiation ? FindAction(negotiation, Addresses::DiplomaticAction_Peace) : nullptr;
    const bool peaceInDeal = peace && (*reinterpret_cast<InDeal_t*>(*reinterpret_cast<uint8_t**>(peace) + Addresses::DiplomaticAction_InDealVtableOffset))(peace);
    if (peace && peace[Addresses::DiplomaticAction_Available] == 1 && !peaceInDeal)
    {
        ActionEntry* list = entries();
        const uint32_t n = *count();
        ActionEntry region;
        std::memcpy(&region, &list[regionEntry], sizeof(region));
        std::memmove(&list[regionEntry], &list[regionEntry + 1], (n - regionEntry - 1) * sizeof(ActionEntry));
        *count() = n - 1;
        const int64_t result = MakeActionButtons_fnc(dropdown);
        list = entries();
        std::memmove(&list[regionEntry + 1], &list[regionEntry], (n - regionEntry - 1) * sizeof(ActionEntry));
        std::memcpy(&list[regionEntry], &region, sizeof(region));
        *count() = n;
        spdlog::debug("[RegionTrading] action buttons: at war without peace in the deal, no region entries");
        return result;
    }

    ActionEntry copy{};
    reinterpret_cast<void (*)(void*, const void*)>(Slid(Addresses::UniString_Copy))(copy.text, entries()[regionEntry].text);
    std::memcpy(copy.flags, entries()[regionEntry].flags, sizeof(copy.flags));
    copy.action = entries()[regionEntry].action;
    reinterpret_cast<void (*)(void*, const ActionEntry*)>(Slid(Addresses::Vector_PushActionEntry))(vector, &copy);
    reinterpret_cast<void (*)(void*)>(Slid(Addresses::UniString_dtor))(copy.text);

    const int64_t result = MakeActionButtons_fnc(dropdown);

    void* parent = *reinterpret_cast<void**>(static_cast<uint8_t*>(dropdown) + Addresses::DiplomacyDropdown_ActionButtonParent);
    int seen = 0;
    for (void* button : Children(parent))
    {
        if (HasId(button, "diplomac_action_possible_6") && ++seen == 2)
        {
            troopsButton = button;
        }
    }
    if (troopsButton)
    {
        if (void* label = FindChild(troopsButton, "row_tx"))
        {
            SetText(label, "Region for Troops", false);
        }
        SetText(troopsButton, "Cede land to them for a contingent of their warriors (federates)", true);
    }
    spdlog::debug("[RegionTrading] action buttons: region entry {}, troops button {}", regionEntry, troopsButton);

    // remove the copy (the last entry)
    ActionEntry& last = entries()[*count() - 1];
    reinterpret_cast<void (*)(void*)>(Slid(Addresses::UniString_dtor))(last.text);
    *count() -= 1;
    return result;
}

// Which button opened the region panel. Only clicks (+ 0x10 == 1, as the engine checks) count.
void NoteActionButton(void* event)
{
    auto* e = static_cast<uint8_t*>(event);
    void* button = e ? *reinterpret_cast<void**>(e + 8) : nullptr;
    if (button && e[0x10] == 1)
    {
        pendingMode = troopsButton && button == troopsButton ? 1 : 0;
    }
}

void OnActionButton(void* dropdown, void* event)
{
    NoteActionButton(event);
    OnActionButton_fnc(dropdown, event);
}

void OnActionButtonThunk(void* dropdown, void* event)
{
    NoteActionButton(event);
    OnActionButtonThunk_fnc(dropdown, event);
}

struct FederatesOffer
{
    int count = 0;
    int cost = 0;
    std::string reason;             // when there are none
    std::vector<std::string> units; // their names
};

// The mod's federates handler for the regions ticked on the left:
// function(ai_key, player_key, region_key...) -> count, cost, reason, unit names (one per line)
FederatesOffer AskFederates(void* panel)
{
    FederatesOffer offer;
    lua_State* L = luaState;
    void* root = Root(panel);
    const char* ai = KeyOf(OtherFaction(panel), Addresses::Faction_Record);
    const char* player = KeyOf(LocalFaction(), Addresses::Faction_Record);
    if (!L || !root || !ai || !player)
    {
        offer.reason = "Not available.";
        return offer;
    }
    const auto offerRows = Rows(root, "list_offers");
    std::vector<int32_t> ids;
    for (void* row : selectedRows)
    {
        if (std::find(offerRows.begin(), offerRows.end(), row) != offerRows.end())
        {
            ids.push_back(RowRegionId(row));
        }
    }
    const std::vector<void*> regions = RegionsById(LocalFaction(), ids);

    const int top = LuaRuntime::gettop(L);
    LuaRuntime::getfield(L, LUA_REGISTRYINDEX, "twase_region_federates_handler");
    if (LuaRuntime::type(L, -1) != LUA_TFUNCTION)
    {
        LuaRuntime::settop(L, top);
        offer.reason = "The region trading mod doesn't offer federates.";
        return offer;
    }
    LuaRuntime::pushstring(L, ai);
    LuaRuntime::pushstring(L, player);
    int arguments = 2;
    for (void* region : regions)
    {
        if (const char* key = KeyOf(region, Addresses::Region_Record))
        {
            LuaRuntime::pushstring(L, key);
            arguments++;
        }
    }
    if (LuaRuntime::pcall(L, arguments, 4, 0) != 0)
    {
        spdlog::error("[RegionTrading] federates handler failed: {}", LuaRuntime::tolstring(L, -1, nullptr));
        offer.reason = "Error, see the log.";
    }
    else
    {
        offer.count = LuaRuntime::type(L, -4) == LUA_TNUMBER ? static_cast<int>(LuaRuntime::tonumber(L, -4)) : 0;
        offer.cost = LuaRuntime::type(L, -3) == LUA_TNUMBER ? static_cast<int>(LuaRuntime::tonumber(L, -3)) : 0;
        if (LuaRuntime::type(L, -2) == LUA_TSTRING)
        {
            offer.reason = LuaRuntime::tolstring(L, -2, nullptr);
        }
        if (LuaRuntime::type(L, -1) == LUA_TSTRING)
        {
            const std::string names = LuaRuntime::tolstring(L, -1, nullptr);
            for (size_t start = 0; start < names.size();)
            {
                const size_t end = std::min(names.find('\n', start), names.size());
                offer.units.push_back(names.substr(start, end - start));
                start = end + 1;
            }
        }
    }
    LuaRuntime::settop(L, top);
    return offer;
}

// the text in lines of at most `width` characters
std::vector<std::string> Wrap(const std::string& aText, size_t width)
{
    std::vector<std::string> lines;
    std::string line;
    for (size_t start = 0; start < aText.size();)
    {
        const size_t end = std::min(aText.find(' ', start), aText.size());
        const std::string word = aText.substr(start, end - start);
        if (!line.empty() && line.size() + 1 + word.size() > width)
        {
            lines.push_back(line);
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
        start = end + 1;
    }
    if (!line.empty())
    {
        lines.push_back(line);
    }
    return lines;
}

// The right list in troops mode: the warriors the AI gives for the ticked regions (or why none), as rows without
// checkboxes made from the panel's row template
void UpdateTroopsList(void* panel)
{
    void* root = Root(panel);
    void* box = root ? FindChild(FindChild(root, "list_demands"), "list_box") : nullptr;
    void* rowTemplate = panel ? *reinterpret_cast<void**>(static_cast<uint8_t*>(panel) + Addresses::TradeRegionsPanel_RowTemplate) : nullptr;
    if (!box || !rowTemplate)
    {
        return;
    }
    reinterpret_cast<void (*)(void*)>(Slid(Addresses::ListBox_Clear))(box);

    const FederatesOffer offer = AskFederates(panel);
    std::vector<std::string> lines;
    if (offer.count > 0)
    {
        lines.push_back(fmt::format("{} units, {} gold", offer.count, offer.cost));
        lines.insert(lines.end(), offer.units.begin(), offer.units.end());
    }
    else
    {
        lines = Wrap(offer.reason, 30);
    }

    using CreateRow_t = void* (*)(void*, void*, const TempString*, void*, void*, int32_t, int32_t, void*, bool, void*);
    static const auto createRow = reinterpret_cast<CreateRow_t>(Slid(Addresses::UIComponent_CreateListRow));
    static const auto stringCtor = reinterpret_cast<StringCtor_t>(Slid(Addresses::String_ctor));
    static const auto stringDtor = reinterpret_cast<StringDtor_t>(Slid(Addresses::String_dtor));
    static const auto setVisible = reinterpret_cast<SetVisible_t>(Slid(Addresses::UIComponent_SetVisible));
    const float rowWidth = Width(box);
    for (size_t i = 0; i < lines.size(); i++)
    {
        TempString id;
        stringCtor(&id, fmt::format("troops_{}", i).c_str());
        void* row = createRow(box, rowTemplate, &id, nullptr, nullptr, 0, 0, nullptr, true, nullptr);
        stringDtor(&id);
        if (!row)
        {
            continue;
        }
        SetText(row, lines[i], false);
        Resize(row, rowWidth, Height(row));
        if (void* checkbox = FindChild(row, "checkbox"))
        {
            setVisible(checkbox, false);
        }
    }
    reinterpret_cast<void (*)(void*)>(Slid(Addresses::ListBox_Layout))(box);
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

    // a region term for troops is labelled so (the heading is the action's name, "Gift Region")
    void* negotiation = *reinterpret_cast<void**>(action + Addresses::DiplomaticAction_Negotiation);
    void* heading = row ? FindChild(row, "button_tx") : nullptr;
    if (heading && negotiation && negotiation == troopsNegotiation)
    {
        SetText(heading, "Region for Troops", false);
    }
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
    // the mode: the button that opened the panel, else the deal's region term (reopened from the deal list)
    void* negotiation = action ? *reinterpret_cast<void**>(static_cast<uint8_t*>(action) + Addresses::DiplomaticAction_Negotiation) : nullptr;
    troopsMode = widened && (pendingMode >= 0 ? pendingMode == 1 : negotiation && negotiation == troopsNegotiation);
    pendingMode = -1;
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
    // the titles: troops mode sets its own, gift mode the layout's (they stay set between openings)
    struct Title
    {
        const char* name;
        const char* troops;
        const char* giftKey;
    };
    for (const Title& title : {Title{"tx_title", "Region for Troops", "uied_component_texts_localised_string_tx_title_default_Text_470063"},
                               Title{"offers_title", "Cede Region", "uied_component_texts_localised_string_offers_title_faction_left_Text_470063"},
                               Title{"demands_title", "Their Warriors", "uied_component_texts_localised_string_demands_title_faction_left_Text_3a0005"}})
    {
        void* component = FindChild(root, title.name);
        const std::string text = troopsMode ? title.troops : Scripting::LuaApi::LocalisedText(title.giftKey);
        if (component && !text.empty())
        {
            SetText(component, text, false);
        }
    }
    if (troopsMode)
    {
        UpdateTroopsList(panel);
    }
    return result;
}
} // namespace

void Hooks::NativeRegionTradingHook::SetLuaState(lua_State* L)
{
    luaState = L;
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
        result = MakeActionButtons_fnc.Attach();
    if (result == 0)
        result = OnActionButton_fnc.Attach();
    if (result == 0)
        result = OnActionButtonThunk_fnc.Attach();
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
        result = OnActionButtonThunk_fnc.Detach();
    if (result == 0)
        result = OnActionButton_fnc.Detach();
    if (result == 0)
        result = MakeActionButtons_fnc.Detach();
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

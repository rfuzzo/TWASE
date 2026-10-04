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
// - offers: a human giving to a non-horde can choose any region but the capital and garrisoned settlements
// Demands keep the engine's rule: only regions where your own army stands.
//
// The AI's deal evaluation has no region component (a region scores +0.0). For deals with regions we add a native gift
// component (CAI_DEAL_COMPONENT_GIFT, the one Payments use) worth the region's value in gold, as returned by the Lua
// handler of the region trading mod, so the AI weighs a region like that much money.

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
using InDeal_t = bool (*)(void* action);

RegionList GiftableRegions(void* from, void* to);
float EvaluateDeal(void* negotiation, void* aiContext, void* a, void* b, int32_t* out5, char* out6);
float ScoreComponents(ComponentList* list, void* aiContext, void* a, int64_t* a4, int32_t* a5, char* a6, uint64_t a7);
Hook<decltype(&EvaluateDeal)> EvaluateDeal_fnc(Addresses::Diplo_EvaluateDeal, &EvaluateDeal);
Hook<decltype(&ScoreComponents)> ScoreComponents_fnc(Addresses::Diplo_ScoreComponents, &ScoreComponents);
bool RegionTradePossible(void* a, void* b);

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
    if (isEnabled && to && IsSettledPair(from, to))
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

    void* evaluator = eval.factionA;
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

    static const auto giftCtor = reinterpret_cast<GiftCtor_t>(Addresses::DealComponentGift_ctor + Image::Get()->GetSlide());
    alignas(16) uint8_t gift[Addresses::DealComponentGift_Size] = {};
    giftCtor(gift, eval->aiContext, eval->factionA, eval->factionB, static_cast<int32_t>(gold * valueSign));

    // the list with our gift appended, restored before the evaluator frees the components
    std::vector<ComponentEntry> entries(list->data, list->data + list->count);
    entries.push_back({gift, Addresses::DiplomaticAction_TradeRegions, 0});
    const ComponentList original = *list;
    list->data = entries.data();
    list->count = static_cast<uint32_t>(entries.size());
    list->capacity = list->count;
    const float score = ScoreComponents_fnc(list, aiContext, a, a4, a5, a6, a7);
    *list = original;

    spdlog::debug("[RegionTrading] regions worth {} gold (sign {}) -> deal score {}", gold, valueSign.load(), score);
    return score;
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
        result = RegionTradePossible_fnc.Detach();
    if (result == 0)
        result = GiftableRegions_fnc.Detach();
    isAttached = result != 0;
    return !isAttached;
}

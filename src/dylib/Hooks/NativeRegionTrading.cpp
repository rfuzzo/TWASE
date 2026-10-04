#include "stdafx.hpp"

#include "NativeRegionTrading.hpp"

#include "../Image.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Addresses.hpp"

// Attila's diplomacy has a "Gift Region" option (diplomatic action type 6, permission "regions"), but only to settle a
// horde: the action is available when one side is a horde, and only regions where the horde's army stands can be given.
// With region trading enabled we widen both rules for two settled factions:
// - available: also between a human and a non-horde AI faction, if the human has a region to give
// - offers: a human giving to a non-horde can choose any region but the capital and garrisoned settlements
// Demands keep the engine's rule: only regions where your own army stands.

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

RegionList GiftableRegions(void* from, void* to);
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
} // namespace

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
    auto result = RegionTradePossible_fnc.Detach();
    if (result == 0)
        result = GiftableRegions_fnc.Detach();
    isAttached = result != 0;
    return !isAttached;
}

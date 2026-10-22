#include "Hooks/NativeRegionTrading.hpp"

#include "../App.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Addresses.hpp"

// The region trading hooks (the logic is src/core/Hooks/NativeRegionTrading.cpp). The cdecl functions are hooked with
// the shared functions directly, thiscall functions are hooked as fastcall with an unused edx.

using namespace sdk::Attila;
namespace RT = Hooks::NativeRegionTradingHook;

namespace
{
bool isAttached = false;

// returns the list through a hidden pointer (the first stack argument)
RT::RegionList* __fastcall GiftableRegions(void* from, void* edx, RT::RegionList* out, void* to);
void __fastcall Execute(void* action, void* edx);

Hook<decltype(&GiftableRegions)> GiftableRegions_fnc(Addresses::Faction_GiftableRegions, &GiftableRegions);
Hook<decltype(&RT::Detour::RegionTradePossible)> RegionTradePossible_fnc(Addresses::DiplomaticAction_RegionTradePossible,
                                                                         &RT::Detour::RegionTradePossible);
Hook<decltype(&Execute)> Execute_fnc(Addresses::TradeRegions_Execute, &Execute);
Hook<decltype(&RT::Detour::EvaluateDeal)> EvaluateDeal_fnc(Addresses::Diplo_EvaluateDeal, &RT::Detour::EvaluateDeal);
Hook<decltype(&RT::Detour::ScoreComponents)> ScoreComponents_fnc(Addresses::Diplo_ScoreComponents, &RT::Detour::ScoreComponents);

RT::RegionList* __fastcall GiftableRegions(void* from, void*, RT::RegionList* out, void* to)
{
    *out = RT::Detour::GiftableRegions(from, to);
    return out;
}

void __fastcall Execute(void* action, void*)
{
    RT::Detour::Execute(action);
}
} // namespace

RT::RegionList RT::Original::GiftableRegions(void* from, void* to)
{
    RegionList list{};
    GiftableRegions_fnc(from, nullptr, &list, to);
    return list;
}

bool RT::Original::RegionTradePossible(void* a, void* b)
{
    return RegionTradePossible_fnc(a, b);
}

void RT::Original::Execute(void* action)
{
    Execute_fnc(action, nullptr);
}

float RT::Original::EvaluateDeal(void* negotiation, void* aiContext, void* a, void* b, int32_t* out5, char* out6)
{
    return EvaluateDeal_fnc(negotiation, aiContext, a, b, out5, out6);
}

float RT::Original::ScoreComponents(ComponentList* list, void* aiContext, void* a, void* a4, int32_t* a5, char* a6, uintptr_t a7)
{
    return ScoreComponents_fnc(list, aiContext, a, a4, a5, a6, a7);
}

// called inside the App's DetourTransaction
bool Hooks::NativeRegionTradingHook::Attach()
{
    auto result = GiftableRegions_fnc.Attach();
    if (result == NO_ERROR)
        result = RegionTradePossible_fnc.Attach();
    if (result == NO_ERROR)
        result = Execute_fnc.Attach();
    if (result == NO_ERROR)
        result = EvaluateDeal_fnc.Attach();
    if (result == NO_ERROR)
        result = ScoreComponents_fnc.Attach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not attach the native region trading hooks. Detours error code: {}", result);
    }
    else
    {
        spdlog::info("The native region trading hooks were attached");
    }
    isAttached = result == NO_ERROR;
    return isAttached;
}

bool Hooks::NativeRegionTradingHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }
    auto result = ScoreComponents_fnc.Detach();
    if (result == NO_ERROR)
        result = EvaluateDeal_fnc.Detach();
    if (result == NO_ERROR)
        result = Execute_fnc.Detach();
    if (result == NO_ERROR)
        result = RegionTradePossible_fnc.Detach();
    if (result == NO_ERROR)
        result = GiftableRegions_fnc.Detach();
    isAttached = result != NO_ERROR;
    return !isAttached;
}

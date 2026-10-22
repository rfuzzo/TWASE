#include "stdafx.hpp"

#include "Hooks/NativeRegionTrading.hpp"

#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Addresses.hpp"

// The region trading hooks (the logic is src/core/Hooks/NativeRegionTrading.cpp). The engine's functions have the
// plain C ABI, so the hooks call the shared functions directly; only GiftableRegions needs its own.

using namespace sdk::Attila;
namespace RT = Hooks::NativeRegionTradingHook;

namespace
{
bool isAttached = false;

// the engine's region list, returned by value through x8 (the destructor keeps clang on that convention)
struct NativeRegionList
{
    uint32_t capacity;
    uint32_t count;
    void** data;
    ~NativeRegionList() {}
};

NativeRegionList GiftableRegions(void* from, void* to)
{
    const RT::RegionList list = RT::Detour::GiftableRegions(from, to);
    return {list.capacity, list.count, list.data};
}

Hook<decltype(&GiftableRegions)> GiftableRegions_fnc(Addresses::Faction_GiftableRegions, &GiftableRegions);
Hook<decltype(&RT::Detour::RegionTradePossible)> RegionTradePossible_fnc(Addresses::DiplomaticAction_RegionTradePossible,
                                                                         &RT::Detour::RegionTradePossible);
Hook<decltype(&RT::Detour::Execute)> Execute_fnc(Addresses::TradeRegions_Execute, &RT::Detour::Execute);
Hook<decltype(&RT::Detour::EvaluateDeal)> EvaluateDeal_fnc(Addresses::Diplo_EvaluateDeal, &RT::Detour::EvaluateDeal);
Hook<decltype(&RT::Detour::ScoreComponents)> ScoreComponents_fnc(Addresses::Diplo_ScoreComponents, &RT::Detour::ScoreComponents);
Hook<decltype(&RT::Detour::PopulatePanel)> PopulatePanel_fnc(Addresses::TradeRegionsPanel_Populate, &RT::Detour::PopulatePanel);
Hook<decltype(&RT::Detour::OnClick)> OnClick_fnc(Addresses::TradeRegionsPanel_OnClick, &RT::Detour::OnClick);
Hook<decltype(&RT::Detour::SetRegion)> SetRegion_fnc(Addresses::DiplomacyContext_SetRegion, &RT::Detour::SetRegion);
Hook<decltype(&RT::Detour::ListButtonSetup)> ListButtonSetup_fnc(Addresses::DiplomacyListTradeRegionsButton_Setup,
                                                                 &RT::Detour::ListButtonSetup);
Hook<decltype(&RT::Detour::MakeActionButtons)> MakeActionButtons_fnc(Addresses::DiplomacyDropdown_MakeActionButtons,
                                                                     &RT::Detour::MakeActionButtons);
Hook<decltype(&RT::Detour::OnActionButton)> OnActionButton_fnc(Addresses::DiplomacyDropdown_OnActionButton,
                                                               &RT::Detour::OnActionButton);
Hook<decltype(&RT::Detour::OnActionButtonThunk)> OnActionButtonThunk_fnc(Addresses::DiplomacyDropdown_OnActionButtonThunk,
                                                                         &RT::Detour::OnActionButtonThunk);
} // namespace

RT::RegionList RT::Original::GiftableRegions(void* from, void* to)
{
    const NativeRegionList list = GiftableRegions_fnc(from, to);
    return {list.capacity, list.count, list.data};
}

bool RT::Original::RegionTradePossible(void* a, void* b)
{
    return RegionTradePossible_fnc(a, b);
}

void RT::Original::Execute(void* action)
{
    Execute_fnc(action);
}

float RT::Original::EvaluateDeal(void* negotiation, void* aiContext, void* a, void* b, int32_t* out5, char* out6)
{
    return EvaluateDeal_fnc(negotiation, aiContext, a, b, out5, out6);
}

float RT::Original::ScoreComponents(ComponentList* list, void* aiContext, void* a, void* a4, int32_t* a5, char* a6, uintptr_t a7)
{
    return ScoreComponents_fnc(list, aiContext, a, a4, a5, a6, a7);
}

void* RT::Original::PopulatePanel(void* panel, void* action)
{
    return PopulatePanel_fnc(panel, action);
}

uintptr_t RT::Original::OnClick(void* panel, void** event)
{
    return OnClick_fnc(panel, event);
}

void RT::Original::SetRegion(void* context, int32_t regionId, void* demandedFrom, void* deal)
{
    SetRegion_fnc(context, regionId, demandedFrom, deal);
}

void RT::Original::ListButtonSetup(void* button, void* root, void* entry, void* context)
{
    ListButtonSetup_fnc(button, root, entry, context);
}

intptr_t RT::Original::MakeActionButtons(void* dropdown)
{
    return MakeActionButtons_fnc(dropdown);
}

void RT::Original::OnActionButton(void* dropdown, void* event)
{
    OnActionButton_fnc(dropdown, event);
}

void RT::Original::OnActionButtonThunk(void* dropdown, void* event)
{
    OnActionButtonThunk_fnc(dropdown, event);
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

#include "stdafx.hpp"

#include "NativeRegionTrading.hpp"

#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Addresses.hpp"

// Attila still contains Rome II's region trading: the diplomatic action type 6 (DIPLOMATIC_ACTION_TRADE_REGIONS,
// permission "regions" in force_diplomacy), its execution (transfers the regions) and the UI (DiplomacyTradeRegionsPanel,
// offer_button_trade_regions). The negotiation creates the action but never marks it available, so it's missing from the
// "Add Offer/Demand" list. We mark it available right before that list is built, unless diplomacy permissions block it.

using namespace sdk::Attila;

namespace
{
bool isAttached = false;
std::atomic<bool> isEnabled = false;

void* BuildPossibleActions(void* context, void* offers, void* demands);
Hook<decltype(&BuildPossibleActions)> BuildPossibleActions_fnc(Addresses::Diplomacy_BuildPossibleActions, &BuildPossibleActions);

void* BuildPossibleActions(void* context, void* offers, void* demands)
{
    if (isEnabled && context)
    {
        auto* negotiation = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(context) + Addresses::DiplomacyContext_Negotiation);
        if (negotiation)
        {
            const auto count = *reinterpret_cast<uint32_t*>(negotiation + Addresses::Negotiation_ActionCount);
            auto** actions = *reinterpret_cast<uint8_t***>(negotiation + Addresses::Negotiation_Actions);
            // the array holds only the types this campaign uses, so search it by type
            for (uint32_t i = 0; actions && i < count; i++)
            {
                auto* action = actions[i];
                if (!action || *reinterpret_cast<int32_t*>(action + Addresses::DiplomaticAction_Type) != Addresses::DiplomaticAction_TradeRegions)
                {
                    continue;
                }
                if (!action[Addresses::DiplomaticAction_Blocked])
                {
                    action[Addresses::DiplomaticAction_Available] = 1;
                }
                break;
            }
        }
    }
    return BuildPossibleActions_fnc(context, offers, demands);
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
    auto result = BuildPossibleActions_fnc.Attach();
    if (result != 0)
    {
        spdlog::error("Could not attach the native region trading hook. Dobby error code: {}", result);
    }
    else
    {
        spdlog::info("The native region trading hook was attached");
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
    auto result = BuildPossibleActions_fnc.Detach();
    isAttached = result != 0;
    return !isAttached;
}

#pragma once

namespace Hooks::NativeRegionTradingHook
{
bool Attach();
bool Detach();

// Offer the engine's own (disabled) region trading action in the diplomacy "Add Offer/Demand" list. Off by default.
bool IsEnabled();
void SetEnabled(bool aEnabled);
} // namespace Hooks::NativeRegionTradingHook

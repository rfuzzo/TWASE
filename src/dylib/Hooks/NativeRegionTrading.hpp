#pragma once

struct lua_State;

namespace Hooks::NativeRegionTradingHook
{
bool Attach();
bool Detach();

// Offer the engine's own (disabled) region trading action in the diplomacy "Add Offer/Demand" list. Off by default.
bool IsEnabled();
void SetEnabled(bool aEnabled);

// The Lua state whose registry holds the region value handler (twase.set_region_value_handler)
void SetLuaState(lua_State* L);
// dev: the sign of the gold amount given to the gift component (+1 / -1), to find the engine's convention
void SetValueSign(int aSign);
} // namespace Hooks::NativeRegionTradingHook

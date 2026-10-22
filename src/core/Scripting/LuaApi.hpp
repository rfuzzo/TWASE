#pragma once

#include <string>

struct lua_State;

// The `twase` Lua table: native functions for script mods, registered into the campaign's
// EpisodicScriptingEnv after scripting.lua ran and before the mods are loaded.
// Shared by both platforms; which functions a platform registers is set in its sdk/Attila/Features.hpp.
namespace Scripting::LuaApi
{
void Register(lua_State* L);

// The localised text of a loc key in the player's language (UTF-8), empty if missing or outside a campaign.
// Only defined with TWASE_HAS_LOCALISATION.
std::string LocalisedText(const char* aKey);
} // namespace Scripting::LuaApi

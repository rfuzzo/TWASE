#pragma once

struct lua_State;

// The `twase` Lua table: native functions for script mods, registered into the campaign's
// EpisodicScriptingEnv after scripting.lua ran and before the mods are loaded.
namespace Scripting::LuaApi
{
void Register(lua_State* L);
} // namespace Scripting::LuaApi

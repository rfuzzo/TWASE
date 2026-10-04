#pragma once

#include <cstdint>

// The game's Lua 5.1 copy (there is a second, unrelated copy from Feral at 0x100E9xxxx, don't use it)
namespace sdk::Attila::Lua
{
// lua API function addresses (called directly, not hooked)
constexpr uint64_t luaL_loadbuffer = 0x103F137D8;
constexpr uint64_t Lua_tolstring = 0x103F283B8;
constexpr uint64_t Lua_settop = 0x103F270F8;
constexpr uint64_t Lua_gettop = 0x103F270E8;
constexpr uint64_t Lua_getfield = 0x103F291D4;
constexpr uint64_t Lua_pcall = 0x103F2A100;
constexpr uint64_t Lua_next = 0x103F2A550;
constexpr uint64_t Lua_pushnil = 0x103F28C80;
constexpr uint64_t Lua_pushvalue = 0x103F2755C;
constexpr uint64_t Lua_type = 0x103F27654;
constexpr uint64_t Lua_getmetatable = 0x103F295BC;
constexpr uint64_t Lua_remove = 0x103F271A0;

// for C functions registered by TWASE (found via luaB_tonumber, luaopen_base and the game's argument checks)
constexpr uint64_t Lua_pushnumber = 0x103F28C94;  // lua_Number is float in the game's Lua, not double
constexpr uint64_t Lua_tonumber = 0x103F280A0;
constexpr uint64_t Lua_pushstring = 0x103F28D48;
constexpr uint64_t Lua_pushboolean = 0x103F2903C;
constexpr uint64_t Lua_pushcclosure = 0x103F28E80;
constexpr uint64_t Lua_createtable = 0x103F2954C;
constexpr uint64_t Lua_setfield = 0x103F29964;
constexpr uint64_t Lua_touserdata = 0x103F28874;
constexpr uint64_t LuaL_error = 0x103F11E80;
constexpr uint64_t Lua_rawequal = 0x103F27B7C;

} // namespace sdk::Attila::Lua

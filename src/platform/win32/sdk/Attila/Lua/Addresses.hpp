#pragma once

namespace sdk::Attila::Lua
{
	// lua API function RVAs (called directly, not hooked)
	constexpr uint32_t luaL_loadbuffer = 0x012C7E20;
	constexpr uint32_t Lua_tolstring = 0x012C7380;
	constexpr uint32_t Lua_settop = 0x012C7210;
	constexpr uint32_t Lua_gettop = 0x012C67B0;
	constexpr uint32_t Lua_getfield = 0x012C66D0;
	constexpr uint32_t Lua_pcall = 0x012C6B00;
	constexpr uint32_t Lua_next = 0x012C6A50;
	constexpr uint32_t Lua_pushnil = 0x012C6CE0;
	constexpr uint32_t Lua_pushvalue = 0x012C6D80;
	constexpr uint32_t Lua_type = 0x012C74E0;
	constexpr uint32_t Lua_getmetatable = 0x012C6730;
	constexpr uint32_t Lua_remove = 0x012C6F60;

	// for C functions registered by TWASE (found via luaB_tonumber, luaB_tostring, luaB_rawequal, luaopen_base and luaL_checkudata)
	constexpr uint32_t Lua_pushnumber = 0x012C6D00;  // lua_Number is float in the game's Lua, not double
	constexpr uint32_t Lua_tonumber = 0x012C7400;
	constexpr uint32_t Lua_pushstring = 0x012C6D20;
	constexpr uint32_t Lua_pushboolean = 0x012C6B80;
	constexpr uint32_t Lua_pushcclosure = 0x012C6BA0;
	constexpr uint32_t Lua_createtable = 0x012C64E0;
	constexpr uint32_t Lua_setfield = 0x012C70D0;
	constexpr uint32_t Lua_touserdata = 0x012C74B0;
	constexpr uint32_t LuaL_error = 0x012C7BF0;
	constexpr uint32_t Lua_rawequal = 0x012C6DE0;
	constexpr uint32_t Lua_toboolean = 0x012C72E0;   // luaB_tostring

} // namespace sdk::Attila::Lua

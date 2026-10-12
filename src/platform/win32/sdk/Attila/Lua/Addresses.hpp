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

} // namespace sdk::Attila::Lua

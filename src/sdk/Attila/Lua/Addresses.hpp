#pragma once

namespace sdk::Attila::Lua
{
	// lua API function RVAs (called directly, not hooked)
	constexpr uint32_t luaL_loadbuffer = 0x012BFB40;
	constexpr uint32_t Lua_tolstring = 0x012BF0A0;
	constexpr uint32_t Lua_settop = 0x012BEF30;
	constexpr uint32_t Lua_gettop = 0x012BE4D0;
	constexpr uint32_t Lua_getfield = 0x012BE3F0;
	constexpr uint32_t Lua_pcall = 0x012BE820;
	constexpr uint32_t Lua_next = 0x012BE770;
	constexpr uint32_t Lua_pushnil = 0x012BEA00;
	constexpr uint32_t Lua_pushvalue = 0x012BEAA0;
	constexpr uint32_t Lua_type = 0x012BF200;
	constexpr uint32_t Lua_getmetatable = 0x012BE450;
	constexpr uint32_t Lua_remove = 0x012BEC80;

} // namespace sdk::Lua

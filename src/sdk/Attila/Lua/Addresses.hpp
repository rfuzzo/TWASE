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

} // namespace sdk::Lua

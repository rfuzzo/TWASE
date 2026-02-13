#pragma once
#include <cstdint>

namespace Addresses
{
	// hooks
	constexpr DWORD LuaLog = 0x01A4AAD0;
	//constexpr DWORD LuaPcall = 0x012BE820;

	// lua API function RVAs (called directly, not hooked)
	constexpr DWORD luaL_loadbuffer = 0x012BFB40;
	constexpr DWORD Lua_tolstring = 0x012BF0A0;
	constexpr DWORD Lua_settop = 0x012BEF30;
	constexpr DWORD Lua_gettop = 0x012BE4D0;

	constexpr DWORD GetLuaState = 0x012BE4D0;

	// patches
	constexpr DWORD BitSetCrashAddr = 0x01600590;
} // namespace Addresses

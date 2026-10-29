#pragma once

#include "../Build.hpp"

namespace sdk::Attila::Lua
{
	// lua API function RVAs (called directly, not hooked)
	constexpr BuildAddress luaL_loadbuffer{0x012C7E20, 0x012C7E20};
	constexpr BuildAddress Lua_tolstring{0x012C7380, 0x012C7380};
	constexpr BuildAddress Lua_settop{0x012C7210, 0x012C7210};
	constexpr BuildAddress Lua_gettop{0x012C67B0, 0x012C67B0};
	constexpr BuildAddress Lua_getfield{0x012C66D0, 0x012C66D0};
	constexpr BuildAddress Lua_pcall{0x012C6B00, 0x012C6B00};
	constexpr BuildAddress Lua_next{0x012C6A50, 0x012C6A50};
	constexpr BuildAddress Lua_pushnil{0x012C6CE0, 0x012C6CE0};
	constexpr BuildAddress Lua_pushvalue{0x012C6D80, 0x012C6D80};
	constexpr BuildAddress Lua_type{0x012C74E0, 0x012C74E0};
	constexpr BuildAddress Lua_getmetatable{0x012C6730, 0x012C6730};
	constexpr BuildAddress Lua_remove{0x012C6F60, 0x012C6F60};

	// for C functions registered by TWASE (found via luaB_tonumber, luaB_tostring, luaB_rawequal, luaopen_base and luaL_checkudata)
	constexpr BuildAddress Lua_pushnumber{0x012C6D00, 0x012C6D00};  // lua_Number is float in the game's Lua, not double
	constexpr BuildAddress Lua_tonumber{0x012C7400, 0x012C7400};
	constexpr BuildAddress Lua_pushstring{0x012C6D20, 0x012C6D20};
	constexpr BuildAddress Lua_pushboolean{0x012C6B80, 0x012C6B80};
	constexpr BuildAddress Lua_pushcclosure{0x012C6BA0, 0x012C6BA0};
	constexpr BuildAddress Lua_createtable{0x012C64E0, 0x012C64E0};
	constexpr BuildAddress Lua_setfield{0x012C70D0, 0x012C70D0};
	constexpr BuildAddress Lua_touserdata{0x012C74B0, 0x012C74B0};
	constexpr BuildAddress LuaL_error{0x012C7BF0, 0x012C7BF0};
	constexpr BuildAddress Lua_rawequal{0x012C6DE0, 0x012C6DE0};
	constexpr BuildAddress Lua_toboolean{0x012C72E0, 0x012C72E0};   // luaB_tostring

} // namespace sdk::Attila::Lua

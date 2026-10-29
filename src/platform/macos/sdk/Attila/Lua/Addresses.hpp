#pragma once

#include "../Build.hpp"

// The game's Lua 5.1 copy (there is a second, unrelated copy from Feral at 0x100E9xxxx, don't use it)
namespace sdk::Attila::Lua
{
// lua API function addresses (called directly, not hooked)
constexpr BuildAddress luaL_loadbuffer{0x103f137d8, 0x103f12120};
constexpr BuildAddress Lua_tolstring{0x103f283b8, 0x103f26d00};
constexpr BuildAddress Lua_settop{0x103f270f8, 0x103f25a40};
constexpr BuildAddress Lua_gettop{0x103f270e8, 0x103f25a30};
constexpr BuildAddress Lua_getfield{0x103f291d4, 0x103f27b1c};
constexpr BuildAddress Lua_pcall{0x103f2a100, 0x103f28a48};
constexpr BuildAddress Lua_next{0x103f2a550, 0x103f28e98};
constexpr BuildAddress Lua_pushnil{0x103f28c80, 0x103f275c8};
constexpr BuildAddress Lua_pushvalue{0x103f2755c, 0x103f25ea4};
constexpr BuildAddress Lua_type{0x103f27654, 0x103f25f9c};
constexpr BuildAddress Lua_getmetatable{0x103f295bc, 0x103f27f04};
constexpr BuildAddress Lua_remove{0x103f271a0, 0x103f25ae8};

// for C functions registered by TWASE (found via luaB_tonumber, luaopen_base and the game's argument checks)
constexpr BuildAddress Lua_pushnumber{0x103f28c94, 0x103f275dc};  // lua_Number is float in the game's Lua, not double
constexpr BuildAddress Lua_tonumber{0x103f280a0, 0x103f269e8};
constexpr BuildAddress Lua_pushstring{0x103f28d48, 0x103f27690};
constexpr BuildAddress Lua_pushboolean{0x103f2903c, 0x103f27984};
constexpr BuildAddress Lua_pushcclosure{0x103f28e80, 0x103f277c8};
constexpr BuildAddress Lua_createtable{0x103f2954c, 0x103f27e94};
constexpr BuildAddress Lua_setfield{0x103f29964, 0x103f282ac};
constexpr BuildAddress Lua_touserdata{0x103f28874, 0x103f271bc};
constexpr BuildAddress LuaL_error{0x103f11e80, 0x103f107c8};
constexpr BuildAddress Lua_rawequal{0x103f27b7c, 0x103f264c4};
constexpr BuildAddress Lua_toboolean{0x103f282bc, 0x103f26c04};     // luaB_tostring

} // namespace sdk::Attila::Lua

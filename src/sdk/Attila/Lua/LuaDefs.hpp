#pragma once

#include <cstddef>
#include <cstdint>

// Forward-declare the opaque Lua state (Lua 5.1 ABI)
struct lua_State;

// Lua function pointer typedefs
using luaL_loadbuffer_t = int(__cdecl*)(lua_State* L, const char* buffer, size_t size, const char* name);
using lua_tolstring_t   = const char*(__cdecl*)(lua_State* L, int idx, size_t* len);
using lua_settop_t      = void(__cdecl*)(lua_State* L, int idx);
using lua_gettop_t      = int(__cdecl*)(lua_State* L);
using lua_getfield_t	= int(__cdecl*)(lua_State* L, int idx, const char* k);

#define LUA_OK           0
#define LUA_ERRRUN       2
#define LUA_ERRSYNTAX    3
#define LUA_MULTRET     (-1)

/*
** pseudo-indices
*/
#define LUA_REGISTRYINDEX	(-10000)
#define LUA_ENVIRONINDEX	(-10001)
#define LUA_GLOBALSINDEX	(-10002)
#define lua_upvalueindex(i)	(LUA_GLOBALSINDEX-(i))
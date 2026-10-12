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
using lua_pcall_t		= int(__cdecl*)(lua_State* L, int nargs, int nresults, int errfunc);
using lua_next_t		= int(__cdecl*)(lua_State* L, int idx);
using lua_pushnil_t		= void(__cdecl*)(lua_State* L);
using lua_pushvalue_t	= void(__cdecl*)(lua_State* L, int idx);
using lua_type_t		= int(__cdecl*)(lua_State* L, int idx);
using lua_getmetatable_t = int(__cdecl*)(lua_State* L, int idx);
using lua_remove_t		= void(__cdecl*)(lua_State* L, int idx);


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

/*
** basic types
*/
#define LUA_TNONE		(-1)

#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8
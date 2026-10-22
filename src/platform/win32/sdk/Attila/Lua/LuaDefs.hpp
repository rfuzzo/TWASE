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

// The game's Lua is built with LUA_NUMBER = float (lua_pushnumber stores 4 bytes, lua_tonumber returns a float)
using lua_Number = float;
using lua_CFunction = int(__cdecl*)(lua_State* L);

using lua_pushnumber_t   = void(__cdecl*)(lua_State* L, lua_Number n);
using lua_tonumber_t     = lua_Number(__cdecl*)(lua_State* L, int idx);
using lua_pushstring_t   = void(__cdecl*)(lua_State* L, const char* s);
using lua_pushboolean_t  = void(__cdecl*)(lua_State* L, int b);
using lua_pushcclosure_t = void(__cdecl*)(lua_State* L, lua_CFunction fn, int n);
using lua_createtable_t  = void(__cdecl*)(lua_State* L, int narr, int nrec);
using lua_setfield_t     = void(__cdecl*)(lua_State* L, int idx, const char* k);
using lua_touserdata_t   = void*(__cdecl*)(lua_State* L, int idx);
using luaL_error_t       = int(__cdecl*)(lua_State* L, const char* fmt, ...);
using lua_rawequal_t     = int(__cdecl*)(lua_State* L, int idx1, int idx2);
using lua_toboolean_t    = int(__cdecl*)(lua_State* L, int idx);


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
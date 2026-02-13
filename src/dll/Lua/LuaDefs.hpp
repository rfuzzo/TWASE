#pragma once

// Forward-declare the opaque Lua state (Lua 5.1 ABI)
struct lua_State;

// Lua function pointer typedefs (__cdecl is standard Lua calling convention)
using lua_pcall_t       = int(__cdecl*)(lua_State* L, int nargs, int nresults, int errfunc);
using luaL_loadbuffer_t = int(__cdecl*)(lua_State* L, const char* buffer, size_t size, const char* name);
using lua_tolstring_t   = const char*(__cdecl*)(lua_State* L, int idx, size_t* len);
using lua_settop_t      = void(__cdecl*)(lua_State* L, int idx);
using lua_gettop_t      = int(__cdecl*)(lua_State* L);

#define LUA_OK           0
#define LUA_ERRRUN       2
#define LUA_ERRSYNTAX    3
#define LUA_MULTRET     (-1)
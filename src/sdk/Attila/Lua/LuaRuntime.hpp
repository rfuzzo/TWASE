#pragma once

#include "LuaDefs.hpp"
#include <atomic>

/// Resolved Lua function pointers – call these after Init().
class LuaRuntime
{
public:
    static void Init(uint32_t empireDllBase);
    static bool IsReady();

	// override since the game doesn't export this
    static int loadstring(lua_State* L, const char* s);

    // Resolved function pointers (safe to call after Init)
    static luaL_loadbuffer_t    loadbuffer;
    static lua_tolstring_t      tolstring;
    static lua_settop_t         settop;
    static lua_gettop_t         gettop;
	static lua_getfield_t       getfield;
	static lua_pcall_t          pcall;
    static lua_next_t           next;
    static lua_pushnil_t        pushnil;
    static lua_pushvalue_t      pushvalue;
    static lua_type_t           type;
    static lua_getmetatable_t   getmetatable;
    static lua_remove_t         remove;

    static bool istable(lua_State* L, int idx) { return type(L, idx) == LUA_TTABLE; }
    static bool isstring(lua_State* L, int idx) { return type(L, idx) == LUA_TSTRING; }
    static bool isuserdata(lua_State* L, int idx) { return type(L, idx) == LUA_TUSERDATA; }
private:
    static std::atomic<bool>   s_ready;
};
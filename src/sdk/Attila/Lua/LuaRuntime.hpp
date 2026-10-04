#pragma once

#include "LuaDefs.hpp"

#include <atomic>

/// Resolved Lua function pointers – call these after Init().
class LuaRuntime
{
public:
    // aSlide: ASLR slide of the game executable
    static void Init(uintptr_t aSlide);
    static bool IsReady();

    // override since the game doesn't export this
    static int loadstring(lua_State* L, const char* s);

    // Resolved function pointers (safe to call after Init)
    static luaL_loadbuffer_t loadbuffer;
    static lua_tolstring_t tolstring;
    static lua_settop_t settop;
    static lua_gettop_t gettop;
    static lua_getfield_t getfield;
    static lua_pcall_t pcall;
    static lua_next_t next;
    static lua_pushnil_t pushnil;
    static lua_pushvalue_t pushvalue;
    static lua_type_t type;
    static lua_getmetatable_t getmetatable;
    static lua_remove_t remove;
    static lua_pushnumber_t pushnumber;
    static lua_tonumber_t tonumber;
    static lua_pushstring_t pushstring;
    static lua_pushboolean_t pushboolean;
    static lua_pushcclosure_t pushcclosure;
    static lua_createtable_t createtable;
    static lua_setfield_t setfield;
    static lua_touserdata_t touserdata;
    static luaL_error_t error;

    static bool istable(lua_State* L, int idx) { return type(L, idx) == LUA_TTABLE; }
    static bool isstring(lua_State* L, int idx) { return type(L, idx) == LUA_TSTRING; }
    static bool isuserdata(lua_State* L, int idx) { return type(L, idx) == LUA_TUSERDATA; }

private:
    static std::atomic<bool> s_ready;
};

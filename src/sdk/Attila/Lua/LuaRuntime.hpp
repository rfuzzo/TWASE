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
    static luaL_loadbuffer_t  loadbuffer;
    static lua_tolstring_t    tolstring;
    static lua_settop_t       settop;
    static lua_gettop_t       gettop;
	static lua_getfield_t     getfield;
	static lua_pcall_t        pcall;
private:
    static std::atomic<bool>   s_ready;
};
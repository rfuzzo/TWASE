#pragma once
#include "LuaDefs.hpp"

/// Resolved Lua function pointers – call these after Init().
class LuaRuntime
{
public:
    static void Init(DWORD empireDllBase);
    static bool IsReady();

    // 
    static int loadstring(lua_State* L, const char* s);

    /// Captured from the hooked lua_pcall; set once, read many.
    static void         SetState(lua_State* L);
    static lua_State*   GetState();

    // Resolved function pointers (safe to call after Init)
    static lua_pcall_t        pcall;
    static luaL_loadbuffer_t  loadbuffer;
    static lua_tolstring_t    tolstring;
    static lua_settop_t       settop;
    static lua_gettop_t       gettop;

private:
    static lua_State*          s_luaState;
    static std::atomic<bool>   s_ready;
};
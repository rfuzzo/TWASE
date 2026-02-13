#include "LuaRuntime.hpp"
#include "../Hooking/Addresses.hpp"

lua_pcall_t       LuaRuntime::pcall      = nullptr;
luaL_loadbuffer_t LuaRuntime::loadbuffer = nullptr;
lua_tolstring_t   LuaRuntime::tolstring  = nullptr;
lua_settop_t      LuaRuntime::settop     = nullptr;
lua_gettop_t      LuaRuntime::gettop     = nullptr;

lua_State*        LuaRuntime::s_luaState = nullptr;
std::atomic<bool> LuaRuntime::s_ready    = false;

void LuaRuntime::Init(DWORD base)
{
    pcall      = reinterpret_cast<lua_pcall_t>      (base + Addresses::LuaPcall);
    loadbuffer = reinterpret_cast<luaL_loadbuffer_t>(base + Addresses::luaL_loadbuffer);
    tolstring  = reinterpret_cast<lua_tolstring_t>  (base + Addresses::Lua_tolstring);
    settop     = reinterpret_cast<lua_settop_t>     (base + Addresses::Lua_settop);
    gettop     = reinterpret_cast<lua_gettop_t>     (base + Addresses::Lua_gettop);

    spdlog::info("LuaRuntime resolved all function pointers");
}

// define myself since it doesn't exist in the game runtime
int LuaRuntime::loadstring(lua_State* L, const char* s) {
    return loadbuffer(L, s, strlen(s), s);
}

bool LuaRuntime::IsReady()
{
    return s_ready.load(std::memory_order_acquire) && s_luaState != nullptr;
}

void LuaRuntime::SetState(lua_State* L)
{
    if (!s_luaState && L)
    {
        s_luaState = L;
        s_ready.store(true, std::memory_order_release);
        spdlog::info("LuaRuntime captured lua_State* = {}", static_cast<void*>(L));
    }
}

lua_State* LuaRuntime::GetState()
{
    return s_luaState;
}
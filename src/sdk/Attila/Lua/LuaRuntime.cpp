#include "LuaRuntime.hpp"
#include "Addresses.hpp" 

luaL_loadbuffer_t LuaRuntime::loadbuffer = nullptr;
lua_tolstring_t   LuaRuntime::tolstring  = nullptr;
lua_settop_t      LuaRuntime::settop     = nullptr;
lua_gettop_t      LuaRuntime::gettop     = nullptr;
lua_getfield_t    LuaRuntime::getfield   = nullptr;

std::atomic<bool> LuaRuntime::s_ready    = false;

void LuaRuntime::Init(uint32_t base)
{
    loadbuffer = reinterpret_cast<luaL_loadbuffer_t>(base + sdk::Attila::Lua::luaL_loadbuffer);
    tolstring  = reinterpret_cast<lua_tolstring_t>  (base + sdk::Attila::Lua::Lua_tolstring);
    settop     = reinterpret_cast<lua_settop_t>     (base + sdk::Attila::Lua::Lua_settop);
    gettop     = reinterpret_cast<lua_gettop_t>     (base + sdk::Attila::Lua::Lua_gettop);
	getfield   = reinterpret_cast<lua_getfield_t>   (base + sdk::Attila::Lua::Lua_getfield);

    spdlog::info("LuaRuntime resolved all function pointers");

	s_ready.store(true, std::memory_order_release);
}

// define myself since it doesn't exist in the game runtime
int LuaRuntime::loadstring(lua_State* L, const char* s) {
    return loadbuffer(L, s, strlen(s), s);
}

bool LuaRuntime::IsReady()
{
    return s_ready.load(std::memory_order_acquire);
}
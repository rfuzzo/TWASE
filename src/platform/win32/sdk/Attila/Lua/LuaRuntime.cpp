#include "LuaRuntime.hpp"
#include "Addresses.hpp" 

luaL_loadbuffer_t   LuaRuntime::loadbuffer = nullptr;
lua_tolstring_t     LuaRuntime::tolstring  = nullptr;
lua_settop_t        LuaRuntime::settop     = nullptr;
lua_gettop_t        LuaRuntime::gettop     = nullptr;
lua_getfield_t      LuaRuntime::getfield   = nullptr;
lua_pcall_t         LuaRuntime::pcall      = nullptr;
lua_next_t          LuaRuntime::next = nullptr;
lua_pushnil_t       LuaRuntime::pushnil = nullptr;
lua_pushvalue_t     LuaRuntime::pushvalue = nullptr;
lua_type_t          LuaRuntime::type = nullptr;
lua_getmetatable_t  LuaRuntime::getmetatable = nullptr;
lua_remove_t        LuaRuntime::remove = nullptr;


std::atomic<bool> LuaRuntime::s_ready    = false;

void LuaRuntime::Init(uint32_t base)
{
    loadbuffer = reinterpret_cast<luaL_loadbuffer_t>(base + sdk::Attila::Lua::luaL_loadbuffer);
    tolstring  = reinterpret_cast<lua_tolstring_t>  (base + sdk::Attila::Lua::Lua_tolstring);
    settop     = reinterpret_cast<lua_settop_t>     (base + sdk::Attila::Lua::Lua_settop);
    gettop     = reinterpret_cast<lua_gettop_t>     (base + sdk::Attila::Lua::Lua_gettop);
	getfield   = reinterpret_cast<lua_getfield_t>   (base + sdk::Attila::Lua::Lua_getfield);
	pcall      = reinterpret_cast<lua_pcall_t>        (base + sdk::Attila::Lua::Lua_pcall);
	next = reinterpret_cast<lua_next_t>         (base + sdk::Attila::Lua::Lua_next);
	pushnil = reinterpret_cast<lua_pushnil_t>    (base + sdk::Attila::Lua::Lua_pushnil);
	pushvalue = reinterpret_cast<lua_pushvalue_t>(base + sdk::Attila::Lua::Lua_pushvalue);
	type = reinterpret_cast<lua_type_t>         (base + sdk::Attila::Lua::Lua_type);
	getmetatable = reinterpret_cast<lua_getmetatable_t>(base + sdk::Attila::Lua::Lua_getmetatable);
	remove = reinterpret_cast<lua_remove_t>     (base + sdk::Attila::Lua::Lua_remove);

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
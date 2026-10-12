#include "LuaRuntime.hpp"
#include "Addresses.hpp"

#include <cstring>

#include <spdlog/spdlog.h>

luaL_loadbuffer_t LuaRuntime::loadbuffer = nullptr;
lua_tolstring_t LuaRuntime::tolstring = nullptr;
lua_settop_t LuaRuntime::settop = nullptr;
lua_gettop_t LuaRuntime::gettop = nullptr;
lua_getfield_t LuaRuntime::getfield = nullptr;
lua_pcall_t LuaRuntime::pcall = nullptr;
lua_next_t LuaRuntime::next = nullptr;
lua_pushnil_t LuaRuntime::pushnil = nullptr;
lua_pushvalue_t LuaRuntime::pushvalue = nullptr;
lua_type_t LuaRuntime::type = nullptr;
lua_getmetatable_t LuaRuntime::getmetatable = nullptr;
lua_remove_t LuaRuntime::remove = nullptr;
lua_pushnumber_t LuaRuntime::pushnumber = nullptr;
lua_tonumber_t LuaRuntime::tonumber = nullptr;
lua_pushstring_t LuaRuntime::pushstring = nullptr;
lua_pushboolean_t LuaRuntime::pushboolean = nullptr;
lua_pushcclosure_t LuaRuntime::pushcclosure = nullptr;
lua_createtable_t LuaRuntime::createtable = nullptr;
lua_setfield_t LuaRuntime::setfield = nullptr;
lua_touserdata_t LuaRuntime::touserdata = nullptr;
luaL_error_t LuaRuntime::error = nullptr;
lua_rawequal_t LuaRuntime::rawequal = nullptr;
lua_toboolean_t LuaRuntime::toboolean = nullptr;

std::atomic<bool> LuaRuntime::s_ready = false;

namespace
{
template<typename T>
T Resolve(uintptr_t aSlide, uint64_t aAddress)
{
    return reinterpret_cast<T>(aAddress + aSlide);
}
} // namespace

void LuaRuntime::Init(uintptr_t aSlide)
{
    using namespace sdk::Attila;

    loadbuffer = Resolve<luaL_loadbuffer_t>(aSlide, Lua::luaL_loadbuffer);
    tolstring = Resolve<lua_tolstring_t>(aSlide, Lua::Lua_tolstring);
    settop = Resolve<lua_settop_t>(aSlide, Lua::Lua_settop);
    gettop = Resolve<lua_gettop_t>(aSlide, Lua::Lua_gettop);
    getfield = Resolve<lua_getfield_t>(aSlide, Lua::Lua_getfield);
    pcall = Resolve<lua_pcall_t>(aSlide, Lua::Lua_pcall);
    next = Resolve<lua_next_t>(aSlide, Lua::Lua_next);
    pushnil = Resolve<lua_pushnil_t>(aSlide, Lua::Lua_pushnil);
    pushvalue = Resolve<lua_pushvalue_t>(aSlide, Lua::Lua_pushvalue);
    type = Resolve<lua_type_t>(aSlide, Lua::Lua_type);
    getmetatable = Resolve<lua_getmetatable_t>(aSlide, Lua::Lua_getmetatable);
    remove = Resolve<lua_remove_t>(aSlide, Lua::Lua_remove);
    pushnumber = Resolve<lua_pushnumber_t>(aSlide, Lua::Lua_pushnumber);
    tonumber = Resolve<lua_tonumber_t>(aSlide, Lua::Lua_tonumber);
    pushstring = Resolve<lua_pushstring_t>(aSlide, Lua::Lua_pushstring);
    pushboolean = Resolve<lua_pushboolean_t>(aSlide, Lua::Lua_pushboolean);
    pushcclosure = Resolve<lua_pushcclosure_t>(aSlide, Lua::Lua_pushcclosure);
    createtable = Resolve<lua_createtable_t>(aSlide, Lua::Lua_createtable);
    setfield = Resolve<lua_setfield_t>(aSlide, Lua::Lua_setfield);
    touserdata = Resolve<lua_touserdata_t>(aSlide, Lua::Lua_touserdata);
    error = Resolve<luaL_error_t>(aSlide, Lua::LuaL_error);
    rawequal = Resolve<lua_rawequal_t>(aSlide, Lua::Lua_rawequal);
    toboolean = Resolve<lua_toboolean_t>(aSlide, Lua::Lua_toboolean);

    spdlog::info("LuaRuntime resolved all function pointers");

    s_ready.store(true, std::memory_order_release);
}

// define myself since it doesn't exist in the game runtime
int LuaRuntime::loadstring(lua_State* L, const char* s)
{
    return loadbuffer(L, s, strlen(s), s);
}

bool LuaRuntime::IsReady()
{
    return s_ready.load(std::memory_order_acquire);
}

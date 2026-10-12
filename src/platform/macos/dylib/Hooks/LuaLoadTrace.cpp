#include "stdafx.hpp"

#include "LuaLoadTrace.hpp"

#include "../Hooking/Hook.hpp"

#include <pthread.h>

#include "../../sdk/Attila/Lua/Addresses.hpp"
#include "../../sdk/Attila/Lua/LuaDefs.hpp"

namespace
{
int LoadBuffer(lua_State* L, const char* aBuffer, size_t aSize, const char* aName);
Hook<decltype(&LoadBuffer)> LoadBuffer_fnc(sdk::Attila::Lua::luaL_loadbuffer, &LoadBuffer);

std::atomic<uint64_t> loadCount = 0;

int LoadBuffer(lua_State* L, const char* aBuffer, size_t aSize, const char* aName)
{
    auto result = LoadBuffer_fnc(L, aBuffer, aSize, aName);

    auto count = ++loadCount;
    uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    spdlog::trace("[LuaLoad] #{} {} ({} bytes, L {}, thread {}{}) -> {}", count, aName ? aName : "(null)", aSize,
                  static_cast<void*>(L), tid, pthread_main_np() ? " main" : "", result);

    return result;
}
} // namespace

bool Hooks::LuaLoadTraceHook::Attach()
{
    spdlog::trace("Trying to attach the Lua load trace hook at {:#x}...", LoadBuffer_fnc.GetAddress());

    auto result = LoadBuffer_fnc.Attach();
    if (result != 0)
    {
        spdlog::error("Could not attach the Lua load trace hook. Dobby error code: {}", result);
        return false;
    }

    spdlog::info("The Lua load trace hook was attached");
    return true;
}

bool Hooks::LuaLoadTraceHook::Detach()
{
    auto result = LoadBuffer_fnc.Detach();
    if (result != 0)
    {
        spdlog::error("Could not detach the Lua load trace hook. Dobby error code: {}", result);
        return false;
    }

    spdlog::info("The Lua load trace hook was detached after {} loads", loadCount.load());
    return true;
}

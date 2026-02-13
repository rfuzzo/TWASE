#include "LuaPcallHook.hpp"

#include "../App.hpp"
#include "../Lua/LuaRuntime.hpp"
#include "../Hooking/Addresses.hpp"
#include "../Hooking/Hook.hpp"

namespace
{
bool isAttached = false;

int __cdecl Hooked_lua_pcall(lua_State* L, int nargs, int nresults, int errfunc);
Hook<decltype(&Hooked_lua_pcall)> PcallHook(Addresses::LuaPcall, &Hooked_lua_pcall);

int __cdecl Hooked_lua_pcall(lua_State* L, int nargs, int nresults, int errfunc)
{
    // Capture the lua_State* on the very first call
    LuaRuntime::SetState(L);

    // Pass through to the original
    return PcallHook(L, nargs, nresults, errfunc);
}
} // namespace

bool Hooks::LuaPcallHook::Attach()
{
    spdlog::trace("Trying to attach lua_pcall hook at {:#x}...", PcallHook.GetAddress());

    auto result = PcallHook.Attach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not attach lua_pcall hook. Detour error code: {}", result);
    }
    else
    {
        spdlog::info("lua_pcall hook attached");
    }

    isAttached = result == NO_ERROR;
    return isAttached;
}

bool Hooks::LuaPcallHook::Detach()
{
    if (!isAttached)
        return false;

    auto result = PcallHook.Detach();
    isAttached = result != NO_ERROR;
    return !isAttached;
}
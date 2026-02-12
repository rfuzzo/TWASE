#include "SetLuaLogger.hpp"

#include "../App.hpp"
#include "../Config.hpp"

#include "../Hooking/Addresses.hpp"
#include "../Hooking/Hook.hpp"

namespace
{
bool isAttached = false;

void* _SetLuaLogger(FILE* sink);
Hook<decltype(&_SetLuaLogger)> SetLuaLogger_fnc(Addresses::SetLuaLogger, &_SetLuaLogger);

void* _SetLuaLogger(FILE* sink)
{
    if (Config::Get() && Config::Get()->GetScripting().enableLogging)
    {
		spdlog::info("Setting the Lua logger to stdout");
        // invoke the original function with null, that forces the engine to log bLuaPrint to stdout
        return SetLuaLogger_fnc(nullptr);
    }
    else
    {
        return SetLuaLogger_fnc(sink);
    }
}
} // namespace

bool Hooks::SetLuaLoggerHook::Attach()
{
    spdlog::trace("Trying to attach the hook for init scripts at {:#x}...", SetLuaLogger_fnc.GetAddress());

    auto result = SetLuaLogger_fnc.Attach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not attach the hook for init scripts. Detour error code: {}", result);
    }
    else
    {
        spdlog::trace("The hook for init scripts was attached");
    }

    isAttached = result == NO_ERROR;
    return isAttached;
}

bool Hooks::SetLuaLoggerHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }

    spdlog::trace("Trying to detach the hook for init scripts at {:#x}...", SetLuaLogger_fnc.GetAddress());

    auto result = SetLuaLogger_fnc.Detach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not detach the hook for init scripts. Detour error code: {}", result);
    }
    else
    {
        spdlog::trace("The hook for init scripts was detached");
    }

    isAttached = result != NO_ERROR;
    return !isAttached;
}

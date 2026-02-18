#include "SetLuaLogger.hpp"

#include "../App.hpp"
#include "../Config.hpp"
#include "../UI/LuaConsole.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Addresses.hpp" 

// empire.dll uses a custom luaBPrint function to log lua messages, it is essentially "fputs or custom sink".
// We hook this and just add our spdlog to it, and pass the buffer back to the original.

namespace
{
bool isAttached = false;

void* _SetLuaLogger(char* Buffer);
Hook<decltype(&_SetLuaLogger)> LuaLog_fnc(sdk::Attila::Addresses::LuaLog, &_SetLuaLogger);

void* _SetLuaLogger(char* Buffer)
{
    if (App::Get() && App::Get()->GetConfig() && App::Get()->GetConfig()->GetScripting().enableLogging)
    {
		// don't log empty messages
		if (Buffer == nullptr || Buffer[0] == '\0' || Buffer[0] == '\n')
        {
            return LuaLog_fnc(Buffer);
		}

		// send to spdlog
        spdlog::debug("[Lua] {}", Buffer);

        // also echo into the ImGui console
        LuaConsole::Get().AddLog(Buffer);
    }

    // call the original to ensure the original logging still works
    return LuaLog_fnc(Buffer);
}
} // namespace

bool Hooks::LuaLogHook::Attach()
{
    spdlog::trace("Trying to attach the hook for the lua print hook at {:#x}...", LuaLog_fnc.GetAddress());

    auto result = LuaLog_fnc.Attach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not attach the hook for the lua print hook. Detour error code: {}", result);
    }
    else
    {
        spdlog::info("The hook for the lua print hook was attached");
    }

    isAttached = result == NO_ERROR;
    return isAttached;
}

bool Hooks::LuaLogHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }

    spdlog::trace("Trying to detach the hook for the lua print hook at {:#x}...", LuaLog_fnc.GetAddress());

    auto result = LuaLog_fnc.Detach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not detach the hook for the lua print hook. Detour error code: {}", result);
    }
    else
    {
        spdlog::trace("The hook for the lua print hook was detached");
    }

    isAttached = result != NO_ERROR;
    return !isAttached;
}

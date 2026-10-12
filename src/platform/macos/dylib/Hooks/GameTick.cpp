#include "stdafx.hpp"

#include "GameTick.hpp"

#include "../Hooking/Hook.hpp"
#include "../UI/LuaConsole.hpp"

#include "../../sdk/Attila/Addresses.hpp"

namespace
{
// returns true when the run loop should stop
bool GameTick(void* aApp, int32_t a2, int32_t a3, int32_t a4, int32_t a5);
Hook<decltype(&GameTick)> GameTick_fnc(sdk::Attila::Addresses::GameTick, &GameTick);

bool GameTick(void* aApp, int32_t a2, int32_t a3, int32_t a4, int32_t a5)
{
    auto result = GameTick_fnc(aApp, a2, a3, a4, a5);

    // between frames no game script is running, safe to use the Lua states
    LuaConsole::Get().ProcessPending();

    return result;
}
} // namespace

bool Hooks::GameTickHook::Attach()
{
    spdlog::trace("Trying to attach the game tick hook at {:#x}...", GameTick_fnc.GetAddress());

    auto result = GameTick_fnc.Attach();
    if (result != 0)
    {
        spdlog::error("Could not attach the game tick hook. Dobby error code: {}", result);
        return false;
    }

    spdlog::info("The game tick hook was attached");
    return true;
}

bool Hooks::GameTickHook::Detach()
{
    auto result = GameTick_fnc.Detach();
    if (result != 0)
    {
        spdlog::error("Could not detach the game tick hook. Dobby error code: {}", result);
        return false;
    }

    spdlog::trace("The game tick hook was detached");
    return true;
}

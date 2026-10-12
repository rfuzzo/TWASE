#pragma once

// The game's per-frame tick on the WinMain thread, the thread that runs all game Lua. Work that touches Lua from
// other threads (e.g. console commands from the render thread) is queued and executed here between ticks.
namespace Hooks::GameTickHook
{
bool Attach();
bool Detach();
} // namespace Hooks::GameTickHook

#pragma once

// Logs every Lua chunk the game loads (trace level), useful to see which scripts run in which context
namespace Hooks::LuaLoadTraceHook
{
bool Attach();
bool Detach();
} // namespace Hooks::LuaLoadTraceHook

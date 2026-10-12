#pragma once

// Logs the command line Feral passes to CA's WinMain (it builds it itself: resolution, fullscreen and the enabled
// "mod <pack>;" entries) and the mods in it.
namespace Hooks::WinMainHook
{
bool Attach();
} // namespace Hooks::WinMainHook

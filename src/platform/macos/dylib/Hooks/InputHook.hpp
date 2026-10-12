#pragma once

struct ImGuiIO;

// Keyboard and mouse input for the console overlay.
// An NSEvent local monitor on the main thread toggles the console (key below Esc), swallows game input while the
// console is open and queues the events. The render thread feeds them to ImGui in NewFrame.
namespace Hooks::InputHook
{
// installs the monitor on the main thread, safe to call repeatedly from any thread
void Attach();
void Detach();

// render thread: sets the display size / scale and hands queued events to ImGui
void NewFrame(ImGuiIO& aIO, double aFramebufferWidth, double aFramebufferHeight);
} // namespace Hooks::InputHook

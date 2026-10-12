#pragma once

// Draws the ImGui console over the game. Hooks are Objective-C method swizzles, no game code is patched:
// -[CAMetalLayer nextDrawable] finds the concrete command buffer and drawable classes on the first frame, then their
// present methods are swizzled to render ImGui into the drawable right before it is presented.
namespace Hooks::MetalHook
{
bool Attach();
} // namespace Hooks::MetalHook

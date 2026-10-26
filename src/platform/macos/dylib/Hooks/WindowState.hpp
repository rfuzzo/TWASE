#pragma once

namespace WindowState
{
// Logs the game window's state (frame, content size, scale, fullscreen, minimised, the app active, the Metal
// drawable size) when it changed since the last call. Main thread only.
void LogIfChanged();
} // namespace WindowState

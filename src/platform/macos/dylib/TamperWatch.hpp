#pragma once

// [dev] watch_tamper_state: hardware watchpoints on the state words Feral's startup code guards write
// (docs/macos/integrity-and-quirks.md), to find the code that reads them. Every access is caught on a Mach exception
// port of the accessing thread and logged with its PC; that thread's watchpoints are then cleared until the next Arm().
namespace TamperWatch
{
// Starts the exception server thread (once) and sets the watchpoints and exception port on every thread of the game.
// Call periodically: threads created since the last call get armed, threads that hit a watchpoint get re-armed.
void Arm();
} // namespace TamperWatch

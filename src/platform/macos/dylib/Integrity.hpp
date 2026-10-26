#pragma once

// Diagnostics for Feral's code signature checks: the game validates its own running code (SecCodeCopySelf +
// SecCodeCheckValidity, 0x10039bb70) and its files (SecStaticCodeCheckValidity, 0x10056ef3c). We only log the same
// status. These checks don't see patched bytes; the game's code hashing guards do (docs/macos/integrity-and-quirks.md).
// A Dobby hook on 0x10039c358 (the Mac App Store build's address) crashed the Steam game in a static initializer
// (2026-10-26): in the Steam build that address is in the middle of mod_init_func_92, so we don't hook there.
namespace Integrity
{
// The kernel's code signing flags of the game process (csops), logged as "<step>: flags ..." when they changed since
// the last call. Safe before main().
void LogStatus(std::string_view aStep);

// From the main run loop: every 5 s the window state (Hooks/WindowState), every minute SecCodeCheckValidity on the
// running game (what the game checks), each logged when it changes.
void StartPeriodicCheck();
} // namespace Integrity

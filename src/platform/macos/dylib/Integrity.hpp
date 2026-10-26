#pragma once

// Diagnostics for Feral's code signature checks: the game validates its own running code (SecCodeCopySelf +
// SecCodeCheckValidity, 0x10039c358, also from a static initializer at startup) and its files
// (SecStaticCodeCheckValidity, 0x10056da00). We only log the same status, to find out which of TWASE's patches and
// hooks the game can notice (see [dev] disable in config.ini). A Dobby hook on 0x10039c358 crashes the game in that
// initializer (2026-10-26), so we don't trace the game's own calls.
namespace Integrity
{
// The kernel's code signing flags of the game process (csops), logged as "<step>: flags ..." when they changed since
// the last call. Safe before main().
void LogStatus(std::string_view aStep);

// From the main run loop: every 5 s the window state (Hooks/WindowState), every minute SecCodeCheckValidity on the
// running game (what the game checks), each logged when it changes.
void StartPeriodicCheck();
} // namespace Integrity

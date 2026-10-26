#pragma once

// Diagnostics for Feral's code signature checks: the game validates its own running code (SecCodeCopySelf +
// SecCodeCheckValidity, 0x10039c358) and its files (SecStaticCodeCheckValidity, 0x10056da00). We only log the same
// status, to find out which of TWASE's patches and hooks the game can notice (see [dev] disable in config.ini).
namespace Integrity
{
// The kernel's code signing flags of the game process (csops), logged as "<step>: flags ..." when they changed since
// the last call. Safe before main().
void LogStatus(std::string_view aStep);

// From the main run loop: SecCodeCheckValidity on the running game (what the game checks) a few seconds after the
// start, then every minute, logged when the result changes.
void StartPeriodicCheck();
} // namespace Integrity

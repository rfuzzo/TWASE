#pragma once

// Feral's anti-tamper termination stage (docs/macos/integrity-and-quirks.md): delayed jobs on detached threads that
// crash the game (memory exhaustion, stack exhaustion, ...) and an exit() through the game shell. Nothing here changes
// game code: responses are disarmed through their function-object call slots (data), exit() calls and detached jobs are
// only logged (dyld interposing).
namespace TamperResponse
{
// [dev] disarm_tamper_responses: points the call slots of the known responses at functions that only log. Call before
// the game's static initializers.
void DisarmResponses();
} // namespace TamperResponse

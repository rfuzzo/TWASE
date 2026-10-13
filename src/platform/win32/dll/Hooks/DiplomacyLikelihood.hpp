#pragma once

namespace Hooks::DiplomacyLikelihoodHook
{
bool Attach();
bool Detach();

// show the deal score in the diplomacy likelihood tooltip, can be toggled at runtime
bool IsEnabled();
void SetEnabled(bool aEnabled);

// the last deal score the evaluator produced for the diplomacy screen
float LastScore();
} // namespace Hooks::DiplomacyLikelihoodHook

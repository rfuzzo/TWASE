#pragma once

namespace Hooks::DiplomacyLikelihoodHook
{
bool Attach();
bool Detach();

// show the deal score in the diplomacy likelihood tooltip, can be toggled at runtime
bool IsEnabled();
void SetEnabled(bool aEnabled);
} // namespace Hooks::DiplomacyLikelihoodHook

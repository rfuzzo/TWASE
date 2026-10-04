#pragma once

namespace Hooks::DiplomacyLikelihoodHook
{
bool Attach();
bool Detach();

// show the deal score in the diplomacy likelihood tooltip, can be toggled at runtime
bool IsEnabled();
void SetEnabled(bool aEnabled);

// the DiplomacyDropdown the likelihood was last set on (the open diplomacy screen), may be stale once it closed
void* LastDropdown();
} // namespace Hooks::DiplomacyLikelihoodHook

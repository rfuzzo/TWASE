#include "LuaConsole.hpp"
#include "../Hooks/DiplomacyLikelihood.hpp"

#include <imgui.h>

// Runtime toggles for the built-in game tweaks. The defaults come from the [tweaks] section of config.ini.
void LuaConsole::DrawTweaksTab()
{
    ImGui::TextDisabled("Changes apply to this session, set the defaults in config.ini [tweaks].");
    ImGui::Spacing();

    bool diplomacyDealScore = Hooks::DiplomacyLikelihoodHook::IsEnabled();
    if (ImGui::Checkbox("Diplomacy: show deal score", &diplomacyDealScore))
    {
        Hooks::DiplomacyLikelihoodHook::SetEnabled(diplomacyDealScore);
    }
    ImGui::SetItemTooltip("Adds the AI's deal score to the \"Likelihood of success\" tooltip.\n"
                          "The AI accepts deals with a score of 0 or higher.");
}

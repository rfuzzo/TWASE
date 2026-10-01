#include "LuaConsole.hpp"
#include "../App.hpp"
#include "../Config.hpp"
#include "../Hooks/DiplomacyLikelihood.hpp"

#include <imgui.h>

// Runtime toggles for the built-in game tweaks, changes are saved to the [tweaks] section of config.ini.
void LuaConsole::DrawTweaksTab()
{
    ImGui::TextDisabled("Changes are saved to config.ini.");
    ImGui::Spacing();

    bool diplomacyDealScore = Hooks::DiplomacyLikelihoodHook::IsEnabled();
    if (ImGui::Checkbox("Diplomacy: show deal score", &diplomacyDealScore))
    {
        Hooks::DiplomacyLikelihoodHook::SetEnabled(diplomacyDealScore);

        auto config = App::Get()->GetConfig();
        config->GetTweaks().diplomacyDealScore = diplomacyDealScore;
        if (!config->Save())
        {
            AddLog(LogLevel::Error, "Could not save config.ini, see the TWASE log for details");
        }
    }
    ImGui::SetItemTooltip("Adds the AI's deal score to the \"Likelihood of success\" tooltip.\n"
                          "The AI accepts deals with a score of 0 or higher.");
}

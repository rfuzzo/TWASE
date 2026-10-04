#include "stdafx.hpp"

#include "DiplomacyLikelihood.hpp"

#include <cmath>

#include "../App.hpp"
#include "../Config.hpp"
#include "../Image.hpp"
#include "../Utils.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Addresses.hpp"
#include "../../sdk/Attila/UITypes.hpp"

// The diplomacy panel only shows "Likelihood of success: Low/Moderate/High". The game computes a float deal score
// (the AI accepts deals with score >= 0, the UI buckets it at -4 / +4) but only passes the bucket to the UI.
// We capture the score when the UI estimate is computed and add it to the dy_chance tooltip, e.g. "Deal score: -6".

using namespace sdk::Attila;

namespace
{
bool isAttached = false;
std::atomic<bool> isEnabled = true;

float lastScore = 0.0f;
int lastBucket = 0;
bool hasScore = false;

// game functions called directly
using UIComponent_SetTooltipText_t = void (*)(void* component, const WString* text, bool allStates);
using String_ctor_t = TempString* (*)(TempString* self, const char* utf8);
using String_dtor_t = void (*)(TempString* self);
using String_ToUniString_t = WStringResult (*)(const TempString* src);
using UniString_dtor_t = void (*)(WString* self);

float GetDisplayedDealScore(void* deal, void* ctx, void* factionA, void* factionB);
int GetDealLikelihoodBucket(void* self, void* deal);
void* SetLikelihood(void* self, int likelihood, bool show);

Hook<decltype(&GetDisplayedDealScore)> GetDisplayedDealScore_fnc(Addresses::Diplo_GetDisplayedDealScore,
                                                                 &GetDisplayedDealScore);
Hook<decltype(&GetDealLikelihoodBucket)> GetDealLikelihoodBucket_fnc(Addresses::CAI_GetDealLikelihoodBucket,
                                                                     &GetDealLikelihoodBucket);
Hook<decltype(&SetLikelihood)> SetLikelihood_fnc(Addresses::DiplomacyDropdown_SetLikelihood, &SetLikelihood);

// the UI maps the bucket the same way (2 low, 3 moderate, 4 high), unless it forces the likelihood,
// e.g. a deal that is just a single gift from the player is always shown as high
int BucketToLikelihood(int bucket)
{
    switch (bucket)
    {
    case 0:
    case 1:
    case 2: return -1;
    case 3: return 0;
    case 4: return 1;
    default: return 0;
    }
}

float GetDisplayedDealScore(void* deal, void* ctx, void* factionA, void* factionB)
{
    auto score = GetDisplayedDealScore_fnc(deal, ctx, factionA, factionB);
    lastScore = score;
    hasScore = true;
    return score;
}

int GetDealLikelihoodBucket(void* self, void* deal)
{
    // the bucket function can return early without computing a score, don't show a stale one then
    hasScore = false;
    auto bucket = GetDealLikelihoodBucket_fnc(self, deal);
    lastBucket = bucket;

    if (hasScore)
        spdlog::debug("[Diplomacy] deal {} bucket {} score {:.2f}", deal, bucket, lastScore);
    else
        spdlog::debug("[Diplomacy] deal {} bucket {} (no score)", deal, bucket);

    return bucket;
}

constexpr std::string_view ScoreTooltipPrefix = "\n\nDeal score: ";

std::string ToStdString(const WString* str)
{
    return (str->data && str->length > 0) ? Utils::ToUtf8(str->data, str->length) : std::string();
}

// sets the tooltip through the game's string functions, so the string is allocated with the game allocator
void SetTooltip(void* component, const std::string& text)
{
    const auto image = Image::Get();
    auto stringCtor = image->Resolve<std::remove_pointer_t<String_ctor_t>>(Addresses::String_ctor);
    auto stringDtor = image->Resolve<std::remove_pointer_t<String_dtor_t>>(Addresses::String_dtor);
    auto toUniString = image->Resolve<std::remove_pointer_t<String_ToUniString_t>>(Addresses::String_ToUniString);
    auto uniStringDtor = image->Resolve<std::remove_pointer_t<UniString_dtor_t>>(Addresses::UniString_dtor);
    auto setTooltip =
        image->Resolve<std::remove_pointer_t<UIComponent_SetTooltipText_t>>(Addresses::UIComponent_SetTooltipText);

    TempString utf8;
    stringCtor(&utf8, text.c_str());

    auto tooltip = toUniString(&utf8);
    setTooltip(component, &tooltip.value, false);

    uniStringDtor(&tooltip.value);
    stringDtor(&utf8);
}

// adds the score to the tooltip of the current dy_chance state, or only removes the one we added before if score is null
void UpdateLikelihoodTooltip(void* dropdown, const float* score)
{
    auto component = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(dropdown) + DiplomacyDropdown_DyChanceOffset);
    if (!component)
        return;

    auto state = *reinterpret_cast<uint8_t**>(component + UIComponent_CurrentStateOffset);
    if (!state)
        return;

    // the tooltip is stored per state and falls back to the component tooltip (see the GetTooltipText Lua binding)
    auto tooltip = ToStdString(reinterpret_cast<const WString*>(state + UIState_TooltipOffset));
    if (tooltip.empty())
        tooltip = ToStdString(reinterpret_cast<const WString*>(component + UIComponent_TooltipOffset));

    // strip the line we added last time this state was shown
    auto original = tooltip;
    auto pos = tooltip.rfind(ScoreTooltipPrefix);
    if (pos != std::string::npos)
        tooltip.erase(pos);

    if (score)
    {
        // floor to one decimal so the displayed value is >= 0 exactly when the AI would accept (score >= 0)
        tooltip += fmt::format("{}{:+.1f}", ScoreTooltipPrefix, std::floor(*score * 10.0f) / 10.0f);
    }

    if (tooltip == original)
        return;

    SetTooltip(component, tooltip);
}

void* lastDropdown = nullptr;

void* SetLikelihood(void* self, int likelihood, bool show)
{
    lastDropdown = self;
    auto result = SetLikelihood_fnc(self, likelihood, show);

    // only show the score if the displayed likelihood actually comes from it, not when the UI forced it
    auto fromScore = hasScore && likelihood == BucketToLikelihood(lastBucket);
    spdlog::debug("[Diplomacy] ui likelihood {} show {} from score {}", likelihood, show, fromScore);

    if (self && likelihood != -2)
    {
        // when disabled this still strips a score we added before
        UpdateLikelihoodTooltip(self, isEnabled && fromScore ? &lastScore : nullptr);
    }

    return result;
}
} // namespace

bool Hooks::DiplomacyLikelihoodHook::IsEnabled()
{
    return isEnabled;
}

void Hooks::DiplomacyLikelihoodHook::SetEnabled(bool aEnabled)
{
    isEnabled = aEnabled;
    spdlog::info("Diplomacy deal score tooltip {}", aEnabled ? "enabled" : "disabled");
}

bool Hooks::DiplomacyLikelihoodHook::Attach()
{
    spdlog::trace("Trying to attach the diplomacy likelihood hooks...");

    isEnabled = App::Get()->GetConfig()->GetTweaks().diplomacyDealScore;

    auto result = GetDisplayedDealScore_fnc.Attach();
    if (result == 0)
        result = GetDealLikelihoodBucket_fnc.Attach();
    if (result == 0)
        result = SetLikelihood_fnc.Attach();

    if (result != 0)
    {
        spdlog::error("Could not attach the diplomacy likelihood hooks. Dobby error code: {}", result);
    }
    else
    {
        spdlog::info("The diplomacy likelihood hooks were attached");
    }

    isAttached = result == 0;
    return isAttached;
}

bool Hooks::DiplomacyLikelihoodHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }

    spdlog::trace("Trying to detach the diplomacy likelihood hooks...");

    auto result = SetLikelihood_fnc.Detach();
    if (result == 0)
        result = GetDealLikelihoodBucket_fnc.Detach();
    if (result == 0)
        result = GetDisplayedDealScore_fnc.Detach();

    if (result != 0)
    {
        spdlog::error("Could not detach the diplomacy likelihood hooks. Dobby error code: {}", result);
    }
    else
    {
        spdlog::trace("The diplomacy likelihood hooks were detached");
    }

    isAttached = result != 0;
    return !isAttached;
}

void* Hooks::DiplomacyLikelihoodHook::LastDropdown()
{
    return lastDropdown;
}

#include "DiplomacyLikelihood.hpp"

#include <cmath>

#include "../App.hpp"
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

float lastScore = 0.0f;
int lastBucket = 0;
bool hasScore = false;

// game functions called directly
using UIComponent_SetTooltipText_t = void(__thiscall*)(void* component, const WString* text, bool allStates);
using WString_ctor_t = WString*(__thiscall*)(WString* self, const wchar_t* str);
using WString_dtor_t = void(__thiscall*)(WString* self);

// thiscall functions are hooked as fastcall with an unused edx
float __cdecl GetDisplayedDealScore(void* deal, int ctx, void* factionA, void* factionB);
int __fastcall GetDealLikelihoodBucket(void* self, void* edx, void* deal, int a3);
int __fastcall SetLikelihood(void* self, void* edx, int likelihood, bool show);

Hook<decltype(&GetDisplayedDealScore)> GetDisplayedDealScore_fnc(Addresses::Diplo_GetDisplayedDealScore,
                                                                 &GetDisplayedDealScore);
Hook<decltype(&GetDealLikelihoodBucket)> GetDealLikelihoodBucket_fnc(Addresses::CAI_GetDealLikelihoodBucket,
                                                                     &GetDealLikelihoodBucket);
Hook<decltype(&SetLikelihood)> SetLikelihood_fnc(Addresses::DiplomacyDropdown_SetLikelihood, &SetLikelihood);

// the UI maps the bucket the same way (DiplomacyUI_OnDealLikelihoodEvent), unless it forces the likelihood,
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

float __cdecl GetDisplayedDealScore(void* deal, int ctx, void* factionA, void* factionB)
{
    auto score = GetDisplayedDealScore_fnc(deal, ctx, factionA, factionB);
    lastScore = score;
    hasScore = true;
    return score;
}

int __fastcall GetDealLikelihoodBucket(void* self, void* edx, void* deal, int a3)
{
    // the bucket function can return early without computing a score, don't show a stale one then
    hasScore = false;
    auto bucket = GetDealLikelihoodBucket_fnc(self, edx, deal, a3);
    lastBucket = bucket;

    if (hasScore)
        spdlog::debug("[Diplomacy] deal {} bucket {} score {:.2f}", deal, bucket, lastScore);
    else
        spdlog::debug("[Diplomacy] deal {} bucket {} (no score)", deal, bucket);

    return bucket;
}

constexpr const wchar_t* ScoreTooltipPrefix = L"\n\nDeal score: ";

std::wstring ToStdString(const WString* str)
{
    return (str->data && str->length > 0) ? std::wstring(str->data, str->length) : std::wstring();
}

// adds the score to the tooltip of the current dy_chance state, or only removes the one we added before if score is null
void UpdateLikelihoodTooltip(void* dropdown, const float* score)
{
    auto base = static_cast<uintptr_t>(App::Get()->GetEmpireDllAddr());

    auto component = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(dropdown) + DiplomacyDropdown_DyChanceOffset);
    if (!component)
        return;

    auto state = *reinterpret_cast<uint8_t**>(component + UIComponent_CurrentStateOffset);
    if (!state)
        return;

    // the tooltip is stored per state and falls back to the component tooltip (UIComponent_GetTooltipText)
    auto tooltip = ToStdString(reinterpret_cast<const WString*>(state + UIState_TooltipOffset));
    if (tooltip.empty())
        tooltip = ToStdString(reinterpret_cast<const WString*>(component + UIComponent_TooltipOffset));

    // strip the line we added last time this state was shown
    auto original = tooltip;
    auto pos = tooltip.rfind(ScoreTooltipPrefix);
    if (pos != std::wstring::npos)
        tooltip.erase(pos);

    if (score)
    {
        // floor to one decimal so the displayed value is >= 0 exactly when the AI would accept (score >= 0)
        wchar_t line[64];
        swprintf_s(line, L"%s%+.1f", ScoreTooltipPrefix, std::floor(*score * 10.0f) / 10.0f);
        tooltip += line;
    }

    if (tooltip == original)
        return;

    auto wstringCtor = reinterpret_cast<WString_ctor_t>(base + Addresses::WString_ctor);
    auto wstringDtor = reinterpret_cast<WString_dtor_t>(base + Addresses::WString_dtor);
    auto setTooltip = reinterpret_cast<UIComponent_SetTooltipText_t>(base + Addresses::UIComponent_SetTooltipText);

    WString newTooltip;
    wstringCtor(&newTooltip, tooltip.c_str());
    setTooltip(component, &newTooltip, false);
    wstringDtor(&newTooltip);
}

int __fastcall SetLikelihood(void* self, void* edx, int likelihood, bool show)
{
    auto result = SetLikelihood_fnc(self, edx, likelihood, show);

    // only show the score if the displayed likelihood actually comes from it, not when the UI forced it
    auto fromScore = hasScore && likelihood == BucketToLikelihood(lastBucket);
    spdlog::debug("[Diplomacy] ui likelihood {} show {} from score {}", likelihood, show, fromScore);

    if (self && likelihood != -2)
    {
        UpdateLikelihoodTooltip(self, fromScore ? &lastScore : nullptr);
    }

    return result;
}
} // namespace

bool Hooks::DiplomacyLikelihoodHook::Attach()
{
    spdlog::trace("Trying to attach the diplomacy likelihood hooks...");

    auto result = GetDisplayedDealScore_fnc.Attach();
    if (result == NO_ERROR)
        result = GetDealLikelihoodBucket_fnc.Attach();
    if (result == NO_ERROR)
        result = SetLikelihood_fnc.Attach();

    if (result != NO_ERROR)
    {
        spdlog::error("Could not attach the diplomacy likelihood hooks. Detour error code: {}", result);
    }
    else
    {
        spdlog::info("The diplomacy likelihood hooks were attached");
    }

    isAttached = result == NO_ERROR;
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
    if (result == NO_ERROR)
        result = GetDealLikelihoodBucket_fnc.Detach();
    if (result == NO_ERROR)
        result = GetDisplayedDealScore_fnc.Detach();

    if (result != NO_ERROR)
    {
        spdlog::error("Could not detach the diplomacy likelihood hooks. Detour error code: {}", result);
    }
    else
    {
        spdlog::trace("The diplomacy likelihood hooks were detached");
    }

    isAttached = result != NO_ERROR;
    return !isAttached;
}

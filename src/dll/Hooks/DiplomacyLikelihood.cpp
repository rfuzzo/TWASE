#include "DiplomacyLikelihood.hpp"

#include "../App.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Addresses.hpp"
#include "../../sdk/Attila/UITypes.hpp"

// The diplomacy panel only shows "Likelihood of success: Low/Moderate/High". The game computes a float deal score
// (the AI accepts deals with score >= 0, the UI buckets it at -4 / +4) but only passes the bucket to the UI.
// We capture the score when the UI estimate is computed and append it to the dy_chance text, e.g. "Low [-6.3]".

using namespace sdk::Attila;

namespace
{
bool isAttached = false;

float lastScore = 0.0f;
bool hasScore = false;

// game functions called directly
using UIComponent_SetText_t = void(__thiscall*)(void* component, const WString* text, bool allStates);
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
    return GetDealLikelihoodBucket_fnc(self, edx, deal, a3);
}

void AppendScoreToLikelihoodText(void* dropdown, float score)
{
    auto base = static_cast<uintptr_t>(App::Get()->GetEmpireDllAddr());

    auto component = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(dropdown) + DiplomacyDropdown_DyChanceOffset);
    if (!component)
        return;

    auto state = *reinterpret_cast<uint8_t**>(component + UIComponent_CurrentStateOffset);
    if (!state)
        return;

    // the text is stored per state, strip the suffix we added last time this state was shown
    auto current = reinterpret_cast<const WString*>(state + UIState_TextOffset);
    std::wstring text = (current->data && current->length > 0) ? std::wstring(current->data, current->length) : L"";
    if (!text.empty() && text.back() == L']')
    {
        auto pos = text.rfind(L" [");
        if (pos != std::wstring::npos)
            text.erase(pos);
    }

    wchar_t suffix[32];
    swprintf_s(suffix, L" [%+.1f]", score);
    text += suffix;

    auto wstringCtor = reinterpret_cast<WString_ctor_t>(base + Addresses::WString_ctor);
    auto wstringDtor = reinterpret_cast<WString_dtor_t>(base + Addresses::WString_dtor);
    auto setText = reinterpret_cast<UIComponent_SetText_t>(base + Addresses::UIComponent_SetText);

    WString newText;
    wstringCtor(&newText, text.c_str());
    setText(component, &newText, false);
    wstringDtor(&newText);
}

int __fastcall SetLikelihood(void* self, void* edx, int likelihood, bool show)
{
    auto result = SetLikelihood_fnc(self, edx, likelihood, show);

    if (self && hasScore && likelihood != -2)
    {
        spdlog::debug("[Diplomacy] likelihood {} score {:.2f}", likelihood, lastScore);
        AppendScoreToLikelihoodText(self, lastScore);
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

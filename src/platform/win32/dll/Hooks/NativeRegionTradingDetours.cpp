#include "Hooks/NativeRegionTrading.hpp"

#include "../App.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Addresses.hpp"

// The region trading hooks (the logic is src/core/Hooks/NativeRegionTrading.cpp). The cdecl functions are hooked with
// the shared functions directly, thiscall functions are hooked as fastcall with an unused edx.

using namespace sdk::Attila;
namespace RT = Hooks::NativeRegionTradingHook;

namespace
{
bool isAttached = false;

// returns the list through a hidden pointer (the first stack argument)
RT::RegionList* __fastcall GiftableRegions(void* from, void* edx, RT::RegionList* out, void* to);
void __fastcall Execute(void* action, void* edx);
void* __fastcall PopulatePanel(void* panel, void* edx, void* action);
// the panel's click listener, at panel + TradeRegionsPanel_ClickListener
uintptr_t __fastcall OnClick(void* listener, void* edx, void** event);
void __fastcall SetRegion(void* context, void* edx, int32_t regionId, void* demandedFrom, void* deal);
void __fastcall ListButtonSetup(void* button, void* edx, void* root, void* entry, void* context);
void __fastcall MakeActionButtons(void* dropdown, void* edx);
// the dropdown's listener, at dropdown + DiplomacyDropdown_ActionButtonListener
void __fastcall OnActionButton(void* listener, void* edx, void* event);

Hook<decltype(&GiftableRegions)> GiftableRegions_fnc(Addresses::Faction_GiftableRegions, &GiftableRegions);
Hook<decltype(&RT::Detour::RegionTradePossible)> RegionTradePossible_fnc(Addresses::DiplomaticAction_RegionTradePossible,
                                                                         &RT::Detour::RegionTradePossible);
Hook<decltype(&Execute)> Execute_fnc(Addresses::TradeRegions_Execute, &Execute);
Hook<decltype(&RT::Detour::EvaluateDeal)> EvaluateDeal_fnc(Addresses::Diplo_EvaluateDeal, &RT::Detour::EvaluateDeal);
Hook<decltype(&RT::Detour::ScoreComponents)> ScoreComponents_fnc(Addresses::Diplo_ScoreComponents, &RT::Detour::ScoreComponents);
Hook<decltype(&PopulatePanel)> PopulatePanel_fnc(Addresses::TradeRegionsPanel_Populate, &PopulatePanel);
Hook<decltype(&OnClick)> OnClick_fnc(Addresses::TradeRegionsPanel_OnClick, &OnClick);
Hook<decltype(&SetRegion)> SetRegion_fnc(Addresses::DiplomacyContext_SetRegion, &SetRegion);
Hook<decltype(&ListButtonSetup)> ListButtonSetup_fnc(Addresses::DiplomacyListTradeRegionsButton_Setup, &ListButtonSetup);
Hook<decltype(&MakeActionButtons)> MakeActionButtons_fnc(Addresses::DiplomacyDropdown_MakeActionButtons, &MakeActionButtons);
Hook<decltype(&OnActionButton)> OnActionButton_fnc(Addresses::DiplomacyDropdown_OnActionButton, &OnActionButton);

RT::RegionList* __fastcall GiftableRegions(void* from, void*, RT::RegionList* out, void* to)
{
    *out = RT::Detour::GiftableRegions(from, to);
    return out;
}

void __fastcall Execute(void* action, void*)
{
    RT::Detour::Execute(action);
}

void* __fastcall PopulatePanel(void* panel, void*, void* action)
{
    return RT::Detour::PopulatePanel(panel, action);
}

uintptr_t __fastcall OnClick(void* listener, void*, void** event)
{
    return RT::Detour::OnClick(static_cast<uint8_t*>(listener) - Addresses::TradeRegionsPanel_ClickListener, event);
}

void __fastcall SetRegion(void* context, void*, int32_t regionId, void* demandedFrom, void* deal)
{
    RT::Detour::SetRegion(context, regionId, demandedFrom, deal);
}

void __fastcall ListButtonSetup(void* button, void*, void* root, void* entry, void* context)
{
    RT::Detour::ListButtonSetup(button, root, entry, context);
}

void __fastcall MakeActionButtons(void* dropdown, void*)
{
    RT::Detour::MakeActionButtons(dropdown);
}

void __fastcall OnActionButton(void* listener, void*, void* event)
{
    RT::Detour::OnActionButton(static_cast<uint8_t*>(listener) - Addresses::DiplomacyDropdown_ActionButtonListener, event);
}
} // namespace

RT::RegionList RT::Original::GiftableRegions(void* from, void* to)
{
    RegionList list{};
    GiftableRegions_fnc(from, nullptr, &list, to);
    return list;
}

bool RT::Original::RegionTradePossible(void* a, void* b)
{
    return RegionTradePossible_fnc(a, b);
}

void RT::Original::Execute(void* action)
{
    Execute_fnc(action, nullptr);
}

float RT::Original::EvaluateDeal(void* negotiation, void* aiContext, void* a, void* b, int32_t* out5, char* out6)
{
    return EvaluateDeal_fnc(negotiation, aiContext, a, b, out5, out6);
}

float RT::Original::ScoreComponents(ComponentList* list, void* aiContext, void* a, void* a4, int32_t* a5, char* a6, uintptr_t a7)
{
    return ScoreComponents_fnc(list, aiContext, a, a4, a5, a6, a7);
}

void* RT::Original::PopulatePanel(void* panel, void* action)
{
    return PopulatePanel_fnc(panel, nullptr, action);
}

uintptr_t RT::Original::OnClick(void* panel, void** event)
{
    return OnClick_fnc(static_cast<uint8_t*>(panel) + Addresses::TradeRegionsPanel_ClickListener, nullptr, event);
}

void RT::Original::SetRegion(void* context, int32_t regionId, void* demandedFrom, void* deal)
{
    SetRegion_fnc(context, nullptr, regionId, demandedFrom, deal);
}

void RT::Original::ListButtonSetup(void* button, void* root, void* entry, void* context)
{
    ListButtonSetup_fnc(button, nullptr, root, entry, context);
}

intptr_t RT::Original::MakeActionButtons(void* dropdown)
{
    MakeActionButtons_fnc(dropdown, nullptr);
    return 0;
}

void RT::Original::OnActionButton(void* dropdown, void* event)
{
    OnActionButton_fnc(static_cast<uint8_t*>(dropdown) + Addresses::DiplomacyDropdown_ActionButtonListener, nullptr, event);
}

// Windows has one click handler (Mac also has a thunk through another base); not hooked
void RT::Original::OnActionButtonThunk(void* dropdown, void* event)
{
    Original::OnActionButton(dropdown, event);
}

// the engine's inlined push (0x14ED890): doubles the capacity when full, WStrings move by copying their bytes
void RT::Engine::PushActionEntry(void* vector, const void* entry)
{
    struct Vector
    {
        uint32_t capacity;
        uint32_t count;
        uint8_t* data;
    };
    constexpr uint32_t size = Addresses::ActionEntry_Action + sizeof(void*);
    auto* v = static_cast<Vector*>(vector);
    if (v->count >= v->capacity)
    {
        const uint32_t capacity = v->capacity ? 2 * v->capacity : 1;
        const auto alloc = reinterpret_cast<uint8_t* (*)(int32_t)>(App::Get()->GetEmpireDllAddr() + Addresses::ActionEntry_Alloc);
        uint8_t* data = alloc(static_cast<int32_t>(capacity));
        if (v->data)
        {
            std::memcpy(data, v->data, v->count * size);
            reinterpret_cast<void (*)(void*)>(App::Get()->GetEmpireDllAddr() + Addresses::tw_free)(v->data);
        }
        v->data = data;
        v->capacity = capacity;
    }
    uint8_t* slot = v->data + v->count * size;
    const auto* source = static_cast<const uint8_t*>(entry);
    using Copy_t = void(__thiscall*)(void* self, const void* other);
    reinterpret_cast<Copy_t>(App::Get()->GetEmpireDllAddr() + Addresses::UniString_Copy)(slot, source);
    std::memcpy(slot + Addresses::UniString_Size, source + Addresses::UniString_Size, size - Addresses::UniString_Size);
    v->count++;
}

void RT::Engine::Resize(void* component, float width, float height)
{
    using Resize_t = void(__thiscall*)(void*, float, float, bool, bool);
    auto resize = *reinterpret_cast<Resize_t*>(*static_cast<uint8_t**>(component) + Addresses::UIComponent_ResizeVtableOffset);
    resize(component, width, height, true, true);
}

// called inside the App's DetourTransaction
bool Hooks::NativeRegionTradingHook::Attach()
{
    auto result = GiftableRegions_fnc.Attach();
    if (result == NO_ERROR)
        result = RegionTradePossible_fnc.Attach();
    if (result == NO_ERROR)
        result = Execute_fnc.Attach();
    if (result == NO_ERROR)
        result = EvaluateDeal_fnc.Attach();
    if (result == NO_ERROR)
        result = ScoreComponents_fnc.Attach();
    if (result == NO_ERROR)
        result = PopulatePanel_fnc.Attach();
    if (result == NO_ERROR)
        result = OnClick_fnc.Attach();
    if (result == NO_ERROR)
        result = SetRegion_fnc.Attach();
    if (result == NO_ERROR)
        result = ListButtonSetup_fnc.Attach();
    if (result == NO_ERROR)
        result = MakeActionButtons_fnc.Attach();
    if (result == NO_ERROR)
        result = OnActionButton_fnc.Attach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not attach the native region trading hooks. Detours error code: {}", result);
    }
    else
    {
        spdlog::info("The native region trading hooks were attached");
    }
    isAttached = result == NO_ERROR;
    return isAttached;
}

bool Hooks::NativeRegionTradingHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }
    auto result = OnActionButton_fnc.Detach();
    if (result == NO_ERROR)
        result = MakeActionButtons_fnc.Detach();
    if (result == NO_ERROR)
        result = ListButtonSetup_fnc.Detach();
    if (result == NO_ERROR)
        result = SetRegion_fnc.Detach();
    if (result == NO_ERROR)
        result = OnClick_fnc.Detach();
    if (result == NO_ERROR)
        result = PopulatePanel_fnc.Detach();
    if (result == NO_ERROR)
        result = ScoreComponents_fnc.Detach();
    if (result == NO_ERROR)
        result = EvaluateDeal_fnc.Detach();
    if (result == NO_ERROR)
        result = Execute_fnc.Detach();
    if (result == NO_ERROR)
        result = RegionTradePossible_fnc.Detach();
    if (result == NO_ERROR)
        result = GiftableRegions_fnc.Detach();
    isAttached = result != NO_ERROR;
    return !isAttached;
}

#pragma once

#include <cstdint>

#include "Attila/Features.hpp"

struct lua_State;

// The logic is shared (NativeRegionTrading.cpp). Each platform hooks the engine functions in its
// Hooks/NativeRegionTradingDetours.cpp: its hooks call the Detour functions, and it implements Original (its
// trampolines), Engine and Attach / Detach. The signatures leave out the platform's calling conventions.
namespace Hooks::NativeRegionTradingHook
{
bool Attach();
bool Detach();

// Offer the engine's own (disabled) region trading action in the diplomacy "Add Offer/Demand" list. Off by default.
bool IsEnabled();
void SetEnabled(bool aEnabled);

// The Lua state whose registry holds the region value handler (twase.enable_region_trading)
void SetLuaState(lua_State* L);

// the engine's region list
struct RegionList
{
    uint32_t capacity;
    uint32_t count;
    void** data; // REGION*
};

// the deal evaluator's CAI_DEAL_COMPONENT_* list: 16-byte entries on arm64, 8 on x86
struct ComponentEntry
{
    void* component;
    int32_t index; // the position of its action in the negotiation's action array
};

struct ComponentList
{
    uint32_t capacity;
    uint32_t count;
    ComponentEntry* data;
};

#define TWASE_REGION_TRADING_FUNCTIONS                                                                                 \
    RegionList GiftableRegions(void* from, void* to);                                                                  \
    bool RegionTradePossible(void* a, void* b);                                                                        \
    void Execute(void* action);                                                                                        \
    float EvaluateDeal(void* negotiation, void* aiContext, void* a, void* b, int32_t* out5, char* out6);               \
    float ScoreComponents(ComponentList* list, void* aiContext, void* a, void* a4, int32_t* a5, char* a6, uintptr_t a7);

// the region panel and the deal list
#define TWASE_REGION_TRADING_PANEL_FUNCTIONS                                                                           \
    void* PopulatePanel(void* panel, void* action);                                                                    \
    uintptr_t OnClick(void* panel, void** event);                                                                      \
    void SetRegion(void* context, int32_t regionId, void* demandedFrom, void* deal);                                   \
    void ListButtonSetup(void* button, void* root, void* entry, void* context);

// Region for Troops
#define TWASE_REGION_FOR_TROOPS_FUNCTIONS                                                                              \
    intptr_t MakeActionButtons(void* dropdown);                                                                        \
    void OnActionButton(void* dropdown, void* event);                                                                  \
    void OnActionButtonThunk(void* dropdown, void* event);

namespace Detour
{
TWASE_REGION_TRADING_FUNCTIONS
#if TWASE_HAS_REGION_TRADING_PANEL
TWASE_REGION_TRADING_PANEL_FUNCTIONS
#endif
#if TWASE_HAS_REGION_FOR_TROOPS
TWASE_REGION_FOR_TROOPS_FUNCTIONS
#endif
} // namespace Detour

namespace Original
{
TWASE_REGION_TRADING_FUNCTIONS
#if TWASE_HAS_REGION_TRADING_PANEL
TWASE_REGION_TRADING_PANEL_FUNCTIONS
#endif
#if TWASE_HAS_REGION_FOR_TROOPS
TWASE_REGION_FOR_TROOPS_FUNCTIONS
#endif
} // namespace Original

#if TWASE_HAS_REGION_TRADING_PANEL
// engine calls whose arguments differ per platform, also implemented by the platform
namespace Engine
{
// UIComponent's virtual Resize, the way the Lua Resize calls it
void Resize(void* component, float width, float height);
} // namespace Engine
#endif

#undef TWASE_REGION_TRADING_FUNCTIONS
#undef TWASE_REGION_TRADING_PANEL_FUNCTIONS
#undef TWASE_REGION_FOR_TROOPS_FUNCTIONS
} // namespace Hooks::NativeRegionTradingHook

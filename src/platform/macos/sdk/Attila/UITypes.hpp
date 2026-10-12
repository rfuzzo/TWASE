#pragma once

#include <cstdint>

#include "VFSTypes.hpp"

namespace sdk::Attila
{
// CA::UniString, game wide UTF-16 string allocated with the game allocator.
// Build one with String_ctor + String_ToUniString, free it with UniString_dtor.
struct WString
{
    uint32_t length;   // +0x00
    uint32_t capacity; // +0x04
    char16_t* data;    // +0x08
};
static_assert(sizeof(WString) == 0x10);

// String_ToUniString returns the string by value through x8 (indirect result), the non-trivial destructor makes
// clang use the same convention when calling it through a function pointer. Free value with UniString_dtor.
struct WStringResult
{
    WString value;
    ~WStringResult() {}
};

// UIComponent
constexpr uint32_t UIComponent_CurrentStateOffset = 0x140; // UIState*
constexpr uint32_t UIComponent_TooltipOffset = 0x1C0;      // WString, used when the state has no tooltip

// UIState
constexpr uint32_t UIState_NameOffset = 0x20;              // TempString
constexpr uint32_t UIState_TooltipOffset = 0x60;           // WString

// UIDLL::DiplomacyDropdown (the diplomacy negotiation panel)
constexpr uint32_t DiplomacyDropdown_DyChanceOffset = 0x160; // UIComponent* "dy_chance"

} // namespace sdk::Attila

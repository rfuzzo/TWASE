#pragma once

namespace sdk::Attila
{
    // game wide string, allocated with the game allocator, use WString_ctor / WString_dtor
    struct WString {
        unsigned int length;
        unsigned int capacity;
        wchar_t* data;
    };

    // UIComponent
    constexpr uint32_t UIComponent_CurrentStateOffset = 0xB4;   // UIState*

    // UIState
    constexpr uint32_t UIState_TextOffset = 0x38;               // WString

    // DiplomacyDropdown (the diplomacy negotiation panel)
    constexpr uint32_t DiplomacyDropdown_DyChanceOffset = 0xBC; // UIComponent* "dy_chance"

} // namespace sdk::Attila

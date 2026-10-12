#include "stdafx.hpp"

#include "Patches.hpp"

#include "../Hooking/Memory.hpp"
#include "../../sdk/Attila/Addresses.hpp"

void Patches::ApplyPatches()
{
    spdlog::info("Applying patches...");

    ApplyUnitSizePatch();
}

/// Patches a crash when too many units are spawned (e.g. with the mod Fireforged Empire installed).
///
/// The function builds a std::bitset<64> mask (all bits set, a virtual callback clears the entries to exclude) and
/// walks a list of entries, testing bit `index` for each one. With more than 64 entries libc++ throws
/// out_of_range("bitset test argument out of range"), which crashes the game.
///
///   101a51c54  cmp   x20, #0x40
///   101a51c58  b.eq  0x101a52120     ; index == 64 -> throw
///   101a51c5c  ldr   x8, [sp, #0x28]
///   101a51c60  lsr   x8, x8, x20
///   101a51c64  tbz   w8, #0, 101a51c3c ; bit clear -> exclude, set -> include (101a51c68)
///
/// Same fix as on Windows: only the throw branch is retargeted to the include branch, so entries 0-63 behave as
/// before and entries 64+ use the default of the mask (included) instead of throwing.
void Patches::ApplyUnitSizePatch()
{
    // b.eq 0x101a52120
    constexpr uint8_t expected[] = {0x40, 0x26, 0x00, 0x54};
    // b.eq 0x101a51c68 ((0x101a51c68 - 0x101a51c58) / 4 = 4)
    constexpr uint8_t patch[] = {0x80, 0x00, 0x00, 0x54};

    Memory::PatchBytes("unit size patch", sdk::Attila::Addresses::BitSetCrashAddr, expected, patch);
}

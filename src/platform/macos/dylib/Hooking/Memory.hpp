#pragma once

namespace Memory
{
// Writes into the game's code (__TEXT). The page is copied into anonymous memory, patched there and mapped over the
// original in one step, so other threads never see a non-executable page. Falls back to a copy-on-write protection
// change if remapping fails.
bool WriteCode(uintptr_t aAddress, const void* aData, size_t aSize);

// Writes aPatch at the (unslid) aAddress only if the bytes there match aExpected, so a game update can't make us
// corrupt code. Returns true if the patch is in place afterwards.
bool PatchBytes(std::string_view aName, uint64_t aAddress, std::span<const uint8_t> aExpected,
                std::span<const uint8_t> aPatch);
} // namespace Memory

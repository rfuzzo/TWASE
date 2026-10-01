# include "Patches.hpp"

#include "../Utils.hpp"
#include "../../sdk/Attila/Addresses.hpp"

namespace
{
// Writes aPatch at aAddress only if the bytes there match aExpected, so a game update can't make us corrupt code.
bool PatchBytes(const char* aName, DWORD aAddress, const BYTE* aExpected, const BYTE* aPatch, size_t aCount)
{
    std::vector<BYTE> current(aCount);
    MemoryUtils::readBytesUnprotected(aAddress, current.data(), aCount);

    if (std::memcmp(current.data(), aPatch, aCount) == 0)
    {
        spdlog::info("{} is already applied at {}", aName, reinterpret_cast<void*>(aAddress));
        return true;
    }

    if (std::memcmp(current.data(), aExpected, aCount) != 0)
    {
        std::string bytes;
        for (auto b : current)
        {
            bytes += fmt::format("{:02X} ", b);
        }

        spdlog::warn("{} skipped: unexpected bytes at {} ({}), the game version is probably not supported", aName,
                     reinterpret_cast<void*>(aAddress), bytes);
        return false;
    }

    MemoryUtils::writeBytesUnprotected(aAddress, aPatch, aCount);
    spdlog::info("Applied {} at {}", aName, reinterpret_cast<void*>(aAddress));
    return true;
}
} // namespace

// Empire patches
void Patches::ApplyEmpirePatches(DWORD empireDllAddr)
{
	spdlog::info("Applying patches to empire.retail.dll...");

	ApplyUnitSizePatch(empireDllAddr);
}

/// <summary>
/// Patches a crash in empire.retail.dll when too many units are spawned.
/// </summary>
/// <remarks>
/// sub_1091C900 builds a std::bitset&lt;64&gt; mask (all bits set, a callback clears the entries to exclude) and
/// iterates a list of entries, calling bitset::test(index) for each one. With more than 64 entries test() throws
/// "invalid bitset&lt;N&gt; position", which crashes the game.
///
///   0091CB57  cmp  edi, 40h
///   0091CB5A  jnb  loc_1091CD10    ; index >= 64 -> throw
///   ...                            ; test the bit, set -> include (0091CB95), clear -> exclude (0091CB8B)
///
/// We only retarget the jnb to the include branch, so entries 0-63 behave as before and entries 64+ use the
/// default of the mask (included) instead of throwing. Nothing else is moved, unlike the previous patch which
/// widened the cmp and overwrote the bit index computation.
/// </remarks>
void Patches::ApplyUnitSizePatch(DWORD empireDllAddr)
{
    // cmp edi, 40h ; jnb loc_1091CD10
    const BYTE expected[] = { 0x83, 0xFF, 0x40, 0x0F, 0x83, 0xB0, 0x01, 0x00, 0x00 };
    // cmp edi, 40h ; jnb loc_1091CB95 (0x91CB95 - 0x91CB60 = 0x35)
    const BYTE patch[]    = { 0x83, 0xFF, 0x40, 0x0F, 0x83, 0x35, 0x00, 0x00, 0x00 };

    PatchBytes("unit size patch", empireDllAddr + sdk::Attila::Addresses::BitSetCrashAddr, expected, patch,
               sizeof(patch));
}

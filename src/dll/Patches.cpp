# include "Patches.hpp"
#include "Utils.hpp"

// Empire patches
void Patches::ApplyEmpirePatches(DWORD empireDllAddr)
{
	spdlog::info("Applying patches to empire.retail.dll...");

	ApplyUnitSizePatch(empireDllAddr);
}

/// <summary>
/// Patches a crash in empire.retail.dll when too many units are spawned.
/// </summary>
/// <param name="empireDllAddr"></param>
void Patches::ApplyUnitSizePatch(DWORD empireDllAddr)
{
   /* 
   >empire.retail.dll
   009150A7: 83->81
   009150A8: FF->FF
   009150A9: 40->00
   009150AA: 0F->01

   009150AB: 83->00
   009150AC: B0->00
   009150AD: 01->0F
   009150AE: 00->83
   
   009150AF: 00->B0
   009150B0: 33->01
   009150B1: D2->00
   009150B2: 33->00
   
   009150B3: C9->33
   009150B4: 8B->D2
   009150B5: C7->33
   009150B6: 83->C9
   */

    const DWORD patchBitSetCrashAddr = 0x009150A7;
    const BYTE patchBitSetCrashArgs0[] = { 
        0x81, 0xFF, 0x00, 0x01, 
		0x00, 0x00, 0x0F, 0x83, 
		0xB0, 0x01, 0x00, 0x00, 
		0x33, 0xD2, 0x33, 0xC9 };

#ifdef _DEBUG
	// debug: read original bytes and print
	BYTE originalBytes[sizeof(patchBitSetCrashArgs0)] = { 0 };
	MemoryUtils::readBytesUnprotected(empireDllAddr + patchBitSetCrashAddr, originalBytes, sizeof(originalBytes));
	spdlog::debug("Original bytes at empire.retail.dll + 0x{:08X}:", patchBitSetCrashAddr);
	for (size_t i = 0; i < sizeof(originalBytes); i++)
	{
		spdlog::debug("  0x{:02X}", originalBytes[i]);
	}
#endif

    MemoryUtils::writeBytesUnprotected(empireDllAddr + patchBitSetCrashAddr, patchBitSetCrashArgs0, sizeof(patchBitSetCrashArgs0));

    spdlog::info("Applied unit size patch to empire.retail.dll at address {}", 
		reinterpret_cast<void*>(empireDllAddr + patchBitSetCrashAddr));
}
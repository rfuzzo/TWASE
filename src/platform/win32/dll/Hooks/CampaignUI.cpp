#include "Hooks/CampaignUI.hpp"

#include "CampaignUI.hpp"
#include "../App.hpp"
#include "../Hooking/DetourTransaction.hpp"
#include "../Hooking/Hook.hpp"
#include "../../sdk/Attila/Addresses.hpp"
#include "../../sdk/Attila/VFSTypes.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <new>
#include <optional>

namespace
{
using namespace sdk::Attila;
constexpr char NativeUrl[] = "http://Atenc.totalwar.com/#";
constexpr char SlotName[] = "building_slot_%d";
static_assert(sizeof(uintptr_t) == 4, "Campaign UI hooks require the game's x86 ABI");

// The available Windows image predates the fixed-RVA table. Resolve these two
// targets in the loaded PE instead of borrowing RVAs from a different revision.
struct Image
{
    uint8_t* base;
    const IMAGE_NT_HEADERS32* headers;

    template<typename Visitor>
    void VisitSections(DWORD required, DWORD excluded, Visitor visitor) const
    {
        const auto* section = IMAGE_FIRST_SECTION(headers);
        for (unsigned i = 0; i < headers->FileHeader.NumberOfSections; ++i, ++section)
        {
            if ((section->Characteristics & required) != required || (section->Characteristics & excluded) != 0)
                continue;
            const auto rva = section->VirtualAddress;
            if (rva >= headers->OptionalHeader.SizeOfImage)
                continue;
            const auto size = std::min(section->Misc.VirtualSize, headers->OptionalHeader.SizeOfImage - rva);
            visitor(base + rva, size);
        }
    }

    const uint8_t* FindUnique(DWORD required, DWORD excluded, const void* bytes, size_t length,
                              size_t alignment = 1) const
    {
        const uint8_t* found = nullptr;
        bool ambiguous = false;
        VisitSections(required, excluded, [&](const uint8_t* begin, size_t size) {
            if (size < length)
                return;
            for (size_t offset = 0; offset <= size - length; offset += alignment)
            {
                if (std::memcmp(begin + offset, bytes, length) == 0)
                {
                    ambiguous |= found != nullptr;
                    found = begin + offset;
                }
            }
        });
        return ambiguous ? nullptr : found;
    }
};

std::optional<Image> GetImage()
{
    auto* base = reinterpret_cast<uint8_t*>(App::Get()->GetEmpireDllAddr());
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return std::nullopt;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return std::nullopt;
    return Image{base, nt};
}

PVOID volatile* g_urlSlot = nullptr;
const char* g_nativeUrl = nullptr;
std::unique_ptr<char[]> g_customUrl;
bool g_ready = false;
using FindChild = void* (__cdecl*)(void*, const TempString*, bool);
using SetVisible = void (__thiscall*)(void*, bool);
FindChild g_findChild = nullptr;
SetVisible g_setVisible = nullptr;
uint32_t g_managedHighWater[2] = {};

uintptr_t __fastcall Initialize(void* callback, void*, void* region);
using InitializeHook = Hook<decltype(&Initialize)>;
std::optional<InitializeHook> g_initialize;

uintptr_t __fastcall Initialize(void* callback, void*, void* region)
{
    auto* bytes = static_cast<uint8_t*>(callback);
    const bool mainCard = bytes[Addresses::SettlementCallback_MainCard] != 0;
    const auto override = CampaignUI::GetMaxSlots(mainCard);
    auto& highWater = g_managedHighWater[mainCard ? 1 : 0];
    if (override <= 0 && highWater == 0)
        return (*g_initialize)(callback, nullptr, region);
    auto& bound = *reinterpret_cast<uint32_t*>(bytes + Addresses::SettlementCallback_MaxSlots);
    const auto nativeBound = bound;
    const auto effectiveBound = override > 0 ? static_cast<uint32_t>(override) : nativeBound;
    highWater = std::max(highWater, std::min(nativeBound, static_cast<uint32_t>(CampaignUI::MaxSlotOverride)));
    highWater = std::max(highWater, static_cast<uint32_t>(std::max(override, 0)));
    bound = effectiveBound;
    const auto result = (*g_initialize)(callback, nullptr, region);
    // Initialize consumes this bound synchronously and never writes it. Leave
    // the engine's original value intact, including on a reused callback.
    bound = nativeBound;
    // Native Initialize never visits children above the new bound. Explicitly
    // hide that previously managed range when decreasing/resetting a reused
    // card. Borrowed TempString keys avoid engine string allocations entirely.
    const auto cleanupBound = std::max(nativeBound, highWater);
    if (effectiveBound >= cleanupBound)
        return result;
    void* root = *reinterpret_cast<void**>(bytes + Addresses::SettlementCallback_Root);
    if (root)
    {
        for (auto slot = effectiveBound; slot < cleanupBound;)
        {
            ++slot;
            char text[25] = "building_slot_";
            const auto formatted = std::to_chars(text + 14, text + sizeof(text) - 1, slot);
            *formatted.ptr = '\0'; // ten digits fit every uint32_t slot index
            const TempString key{static_cast<uint32_t>(formatted.ptr - text), 0, text};
            if (void* child = g_findChild(root, &key, true))
                g_setVisible(child, false);
        }
    }
    return result;
}

bool Resolve(const Image& image, uint32_t& initializeRva, PVOID volatile*& urlSlot)
{
    const auto* url = image.FindUnique(IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE,
                                       NativeUrl, sizeof(NativeUrl));
    const auto* slotName = image.FindUnique(IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE,
                                           SlotName, sizeof(SlotName));
    if (!url || !slotName)
        return false;
    const auto urlAddress = reinterpret_cast<uintptr_t>(url);
    const auto* pointer = image.FindUnique(IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE,
                                          &urlAddress, sizeof(urlAddress), alignof(void*));
    if (!pointer)
        return false;

    // Both encyclopedia URL builders read this global with PUSH [absolute].
    const uint8_t pushPointer[] = {0xff, 0x35};
    size_t consumers = 0;
    image.VisitSections(IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, 0, [&](const uint8_t* begin, size_t size) {
        for (size_t i = 0; i + 6 <= size; ++i)
        {
            if (std::memcmp(begin + i, pushPointer, sizeof(pushPointer)) != 0)
                continue;
            uintptr_t operand;
            std::memcpy(&operand, begin + i + 2, sizeof(operand));
            if (operand == reinterpret_cast<uintptr_t>(pointer))
                ++consumers;
        }
    });
    if (consumers != 2)
        return false;

    // Independently recorded from Initialize: ECX -> ESI, one REGION* stack
    // argument, then callback->region at +0x40. Further checks tie this entry
    // to the building-slot formatter, both bound comparisons, and RET 4.
    constexpr uint8_t entry[] = {
        0x83, 0xec, 0x74, 0x53, 0x55, 0x56, 0x8b, 0xf1, 0x33, 0xdb, 0x57,
        0x8b, 0xbc, 0x24, 0x88, 0x00, 0x00, 0x00, 0x89, 0x5c, 0x24, 0x24, 0x89, 0x7e, 0x40};
    const auto* target = image.FindUnique(IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, 0, entry, sizeof(entry));
    if (!target)
        return false;
    constexpr uint8_t epilogue[] = {0x5f, 0x5e, 0x5d, 0x5b, 0x83, 0xc4, 0x74, 0xc2, 0x04, 0x00};
    constexpr uint8_t initialBound[] = {0x39, 0x5e, 0x48};
    constexpr uint8_t repeatBound[] = {0x3b, 0x5e, 0x48};
    bool verified = false;
    FindChild findChild = nullptr;
    SetVisible setVisible = nullptr;
    image.VisitSections(IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, 0, [&](const uint8_t* begin, size_t size) {
        if (target < begin || target >= begin + size)
            return;
        const auto* end = target + std::min<size_t>(0x2000, begin + size - target);
        const auto* tail = std::search(target, end, std::begin(epilogue), std::end(epilogue));
        const auto slotAddress = reinterpret_cast<uintptr_t>(slotName);
        uint8_t pushSlot[5] = {0x68};
        std::memcpy(pushSlot + 1, &slotAddress, sizeof(slotAddress));
        verified = tail != end &&
            std::search(target, tail, std::begin(initialBound), std::end(initialBound)) != tail &&
            std::search(target, tail, std::begin(repeatBound), std::end(repeatBound)) != tail &&
            std::search(target, tail, std::begin(pushSlot), std::end(pushSlot)) != tail;
        if (!verified)
            return;
        // Recover the existing UI calls from Initialize, validating their own
        // entry instructions rather than using another image's helper RVAs.
        const auto resolveCall = [&](const uint8_t* pattern, size_t length,
                                     const uint8_t* prologue, size_t prologueLength) -> const uint8_t* {
            const auto* site = std::search(target, tail, pattern, pattern + length);
            if (site == tail || tail - site < static_cast<ptrdiff_t>(length + 4))
                return nullptr;
            int32_t displacement;
            std::memcpy(&displacement, site + length, sizeof(displacement));
            const auto address = reinterpret_cast<uintptr_t>(site + length + 4) + displacement;
            const uint8_t* result = nullptr;
            image.VisitSections(IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, 0,
                [&](const uint8_t* code, size_t codeSize) {
                    const auto beginAddress = reinterpret_cast<uintptr_t>(code);
                    if (address >= beginAddress && address - beginAddress < codeSize &&
                        prologueLength <= codeSize - (address - beginAddress) &&
                        std::memcmp(reinterpret_cast<const void*>(address), prologue, prologueLength) == 0)
                        result = reinterpret_cast<const uint8_t*>(address);
                });
            return result;
        };
        constexpr uint8_t lookupCall[] = {0x6a, 0x01, 0x8d, 0x44, 0x24, 0x38, 0x50, 0x57, 0xe8};
        constexpr uint8_t lookupEntry[] = {0x51, 0x56, 0x8b, 0x74, 0x24, 0x10};
        constexpr uint8_t visibleCall[] = {0xff, 0x74, 0x24, 0x10, 0x8b, 0xcf, 0xe8};
        constexpr uint8_t visibleEntry[] = {0x83, 0xec, 0x0c, 0x53, 0x8b, 0x5c, 0x24, 0x14, 0x56, 0x8b, 0xf1};
        findChild = reinterpret_cast<FindChild>(const_cast<uint8_t*>(
            resolveCall(lookupCall, sizeof(lookupCall), lookupEntry, sizeof(lookupEntry))));
        setVisible = reinterpret_cast<SetVisible>(const_cast<uint8_t*>(
            resolveCall(visibleCall, sizeof(visibleCall), visibleEntry, sizeof(visibleEntry))));
        verified = findChild && setVisible;
    });
    if (!verified)
        return false;
    initializeRva = static_cast<uint32_t>(target - image.base);
    urlSlot = reinterpret_cast<PVOID volatile*>(const_cast<uint8_t*>(pointer));
    g_findChild = findChild;
    g_setVisible = setVisible;
    return true;
}
} // namespace

namespace Hooks::CampaignUIHook
{
bool Attach()
{
    if (g_ready)
        return true;
    const auto image = GetImage();
    uint32_t initializeRva = 0;
    PVOID volatile* slot = nullptr;
    if (!image || !Resolve(*image, initializeRva, slot))
    {
        spdlog::error("Campaign UI targets do not match the verified Windows x86 layout");
        return false;
    }
    DetourTransaction transaction;
    if (!transaction.IsValid())
        return false;
    g_initialize.emplace(initializeRva, &Initialize);
    if (const auto error = g_initialize->Attach(); error != NO_ERROR)
    {
        spdlog::error("Could not attach settlement-card hook: {}", error);
        return false;
    }
    if (!transaction.Commit())
        return false;
    g_urlSlot = slot;
    g_nativeUrl = static_cast<const char*>(*slot);
    g_ready = true;
    spdlog::info("Campaign UI resolved: encyclopedia pointer RVA {:#x}, settlement Initialize RVA {:#x}",
                 reinterpret_cast<uintptr_t>(slot) - reinterpret_cast<uintptr_t>(image->base), initializeRva);
    return true;
}

bool IsReady()
{
    return g_ready;
}

const char* GetEncyclopediaUrl()
{
    return g_urlSlot ? static_cast<const char*>(*g_urlSlot) : nullptr;
}

bool SetEncyclopediaUrl(const char* url)
{
    if (!g_ready)
        return false;
    if (!url || !*url)
    {
        InterlockedExchangePointer(g_urlSlot, const_cast<char*>(g_nativeUrl));
        g_customUrl.reset();
        return true;
    }
    const auto length = std::strlen(url) + 1;
    auto replacement = std::unique_ptr<char[]>(new (std::nothrow) char[length]);
    if (!replacement)
        return false;
    std::memcpy(replacement.get(), url, length);
    InterlockedExchangePointer(g_urlSlot, replacement.get());
    g_customUrl = std::move(replacement);
    return true;
}

bool Detach()
{
    if (!g_ready)
        return true;
    if (g_initialize->Detach() != NO_ERROR)
        return false;
    InterlockedExchangePointer(g_urlSlot, const_cast<char*>(g_nativeUrl));
    // Keep storage alive during teardown: an engine reader may already hold it.
    g_ready = false;
    return true;
}
} // namespace Hooks::CampaignUIHook

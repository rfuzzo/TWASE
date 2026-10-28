#include "stdafx.hpp"

#include "Hooks/CampaignUI.hpp"
#include "CampaignUI.hpp"

#include "../Hooking/Hook.hpp"
#include "../../sdk/Attila/Addresses.hpp"
#include "../../sdk/Attila/VFSTypes.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <memory>
#include <new>

using namespace sdk::Attila;

namespace
{
bool isReady = false;
int32_t managedHighWater[2] = {};

struct UrlState
{
    const char* native = nullptr;
    std::unique_ptr<char[]> custom;
    bool customActive = false;
};

// The dylib constructor installs hooks before C++ dynamic initializers. Keep the owner explicitly initialized,
// and alive until process exit (game threads can still read the URL during dylib destruction).
UrlState* urlState = nullptr;

const char*& UrlSlot()
{
    return *Image::Get()->Resolve<const char*>(Addresses::g_EncyclopediaBaseUrl);
}

void* ConstructEncyclopedia(void* self);
intptr_t InitializeSettlement(void* self, void* region);

Hook<decltype(&ConstructEncyclopedia)> ConstructEncyclopedia_fnc(Addresses::Encyclopedia_ctor,
                                                               &ConstructEncyclopedia);
Hook<decltype(&InitializeSettlement)> InitializeSettlement_fnc(Addresses::CampaignSettlement_Initialize,
                                                               &InitializeSettlement);

void* ConstructEncyclopedia(void* self)
{
    // Feral can replace the initial URL with its EncyclopaediaBaseURL preference. Never mistake our custom
    // allocation for the native URL when another helper is constructed.
    UrlSlot() = urlState->native;
    auto* result = ConstructEncyclopedia_fnc(self);
    urlState->native = UrlSlot();
    if (urlState->customActive)
        UrlSlot() = urlState->custom.get();
    return result;
}

struct ScopedSlotBound
{
    int32_t& field;
    int32_t original;

    ScopedSlotBound(void* self, int32_t value)
        : field(*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(self) + Addresses::CampaignSettlement_MaxSlots))
        , original(field)
    {
        field = value;
    }

    ~ScopedSlotBound() { field = original; }
};

void HideUnusedSlots(void* self, int32_t bound, int32_t lastManaged)
{
    if (bound >= lastManaged)
        return;
    auto* component = *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + Addresses::CampaignSettlement_Component);
    auto findChild = Image::Get()->Resolve<void*(void*, const TempString*, bool)>(Addresses::UIComponent_FindChild);
    auto clearSlot = Image::Get()->Resolve<void*(void*)>(Addresses::CampaignBuildingSlot_Clear);
    auto setVisible = Image::Get()->Resolve<void(void*, bool)>(Addresses::UIComponent_SetVisible);

    for (int64_t slot = int64_t(bound) + 1; slot <= lastManaged; ++slot)
    {
        // FindChild only compares length/data; it does not retain or destroy this borrowed descriptor.
        char name[32] = "building_slot_";
        auto result = std::to_chars(name + 14, name + sizeof(name) - 1, slot);
        *result.ptr = '\0';
        const auto length = static_cast<uint32_t>(result.ptr - name);
        const TempString key{length, length, name};
        if (auto* child = findChild(component, &key, true))
        {
            auto* controller = *reinterpret_cast<void**>(static_cast<uint8_t*>(child) + Addresses::UIComponent_Checkbox);
            if (controller)
                clearSlot(controller);
            setVisible(child, false);
        }
    }
}

intptr_t InitializeSettlement(void* self, void* region)
{
    const auto* card = static_cast<const uint8_t*>(self);
    const auto capital = card[Addresses::CampaignSettlement_IsCapital] != 0;
    const auto setting = CampaignUI::GetMaxSlots(capital);
    auto& managed = managedHighWater[capital ? 1 : 0];
    // Untouched settings preserve the exact native path, with no extra child lookup or mutation.
    if (!setting && !managed)
        return InitializeSettlement_fnc(self, region);

    const auto native = *reinterpret_cast<const int32_t*>(card + Addresses::CampaignSettlement_MaxSlots);
    if (setting)
        managed = std::max({managed, std::min(native, CampaignUI::MaxSlotOverride), setting});
    const auto bound = setting ? setting : native;
    const ScopedSlotBound scoped(self, bound);
    const auto result = InitializeSettlement_fnc(self, region);

    // The watermark is per presentation class, never per object address (rows are recycled). A real native
    // bound larger than our override range is preserved, not clamped to the public setting limit.
    HideUnusedSlots(self, bound, std::max(native, managed));
    return result;
}

template<size_t N>
bool Matches(uint64_t address, const uint32_t (&expected)[N])
{
    return std::memcmp(reinterpret_cast<const void*>(Image::Get()->Resolve(address)), expected, sizeof(expected)) == 0;
}
} // namespace

bool Hooks::CampaignUIHook::Attach()
{
    if (isReady)
        return true;

    constexpr uint32_t constructor[] = {0xd10243ff, 0xa9074ff4, 0xa9087bfd, 0x910203fd};
    constexpr uint32_t initialize[] = {0xd10383ff, 0x6d0723e9, 0xa9086ffc, 0xa90967fa};
    constexpr uint32_t clearSlot[] = {0xd10103ff, 0xa90157f6, 0xa9024ff4, 0xa9037bfd};
    constexpr uint32_t urlLoad[] = {0xd0013b88, 0xf9472500};
    constexpr char nativeUrl[] = "http://Atenc.totalwar.com/#";
    if (!Matches(Addresses::Encyclopedia_ctor, constructor) ||
        !Matches(Addresses::CampaignSettlement_Initialize, initialize) ||
        !Matches(Addresses::CampaignBuildingSlot_Clear, clearSlot) ||
        !Matches(Addresses::Encyclopedia_UrlLoadEntry, urlLoad) ||
        !Matches(Addresses::Encyclopedia_UrlLoadPage, urlLoad) ||
        std::memcmp(Image::Get()->Resolve<const char>(Addresses::Encyclopedia_NativeUrl), nativeUrl, sizeof(nativeUrl)) != 0)
    {
        spdlog::error("[CampaignUI] Unexpected game instructions or encyclopedia URL; hooks not installed");
        return false;
    }

    if (!urlState)
    {
        urlState = new (std::nothrow) UrlState;
        if (!urlState)
            return false;
    }
    urlState->native = UrlSlot();

    auto result = ConstructEncyclopedia_fnc.Attach();
    if (result == 0)
        result = InitializeSettlement_fnc.Attach();
    if (result != 0)
    {
        spdlog::error("[CampaignUI] Could not attach campaign UI hooks. Dobby error code: {}", result);
        Detach();
        return false;
    }

    isReady = true;
    spdlog::info("The campaign UI hooks were attached");
    return true;
}

bool Hooks::CampaignUIHook::Detach()
{
    isReady = false;
    if (urlState)
    {
        UrlSlot() = urlState->native;
        // Retain the allocation in case a native reader was already using it, but don't reactivate on reattach.
        urlState->customActive = false;
    }
    const auto initialize = InitializeSettlement_fnc.Detach();
    const auto constructor = ConstructEncyclopedia_fnc.Detach();
    const auto success = initialize == 0 && constructor == 0;
    if (!success)
        spdlog::error("[CampaignUI] Could not detach campaign UI hooks: {}, {}", initialize, constructor);
    return success;
}

bool Hooks::CampaignUIHook::IsReady()
{
    return isReady;
}

const char* Hooks::CampaignUIHook::GetEncyclopediaUrl()
{
    return urlState ? UrlSlot() : nullptr;
}

bool Hooks::CampaignUIHook::SetEncyclopediaUrl(const char* value)
{
    if (!isReady)
        return false;
    if (!value || !*value)
    {
        UrlSlot() = urlState->native;
        urlState->customActive = false;
        urlState->custom.reset();
        return true;
    }

    const auto length = std::strlen(value);
    std::unique_ptr<char[]> replacement(new (std::nothrow) char[length + 1]);
    if (!replacement)
        return false;
    std::memcpy(replacement.get(), value, length + 1);
    // Publish only after allocation/copy succeeded, and before releasing the previous allocation. These controls
    // and the engine's encyclopedia builders run on the WinMain game/Lua thread.
    UrlSlot() = replacement.get();
    urlState->custom = std::move(replacement);
    urlState->customActive = true;
    return true;
}

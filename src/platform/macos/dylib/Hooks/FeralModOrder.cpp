#include "stdafx.hpp"

#include "FeralModOrder.hpp"

#include "../Hooking/Hook.hpp"
#include "../Image.hpp"

#include "../../sdk/Attila/Addresses.hpp"

#include <chrono>
#include <thread>

// Feral builds the game's `mod x.pack;` lines from its mod manager in FeralAppendModsToCommandLine (0x100014f1c). Only
// the launcher UI sorts the entries by their load order, and Feral waits at most 1 s for Steam's Workshop item details
// (the preferences, on/off and order, are only applied once all of them are in). Started without the launcher
// (--skip-launcher), the mods therefore come in the order Steam answered, or Workshop mods are missing.
// LinkPacks is called right before the lines are built (and only with mods enabled): wait for the details there, then
// sort the entries with Feral's own sort, as the launcher does before PLAY. See docs/macos/addresses.md.

namespace
{
using namespace sdk::Attila;

constexpr auto MaxWait = std::chrono::seconds(15);

using SortByOrder_t = void* (*)(void* aModManager);
using NStringCStr_t = const char* (*)(const void* aString);

uint64_t LinkPacks(void* aModManager);
Hook<decltype(&LinkPacks)> LinkPacks_fnc(Addresses::CModManager_LinkPacks, &LinkPacks);

uint8_t* Field(void* aObject, uint64_t aOffset)
{
    return static_cast<uint8_t*>(aObject) + aOffset;
}

volatile bool* Refreshing(void* aModManager)
{
    return reinterpret_cast<volatile bool*>(Field(aModManager, Addresses::CModManager_Refreshing));
}

void LogEnabled(void* aModManager)
{
    auto cstr = reinterpret_cast<NStringCStr_t>(Image::Get()->Resolve(Addresses::NString_CStr));
    auto begin = *reinterpret_cast<uint8_t**>(Field(aModManager, Addresses::CModManager_EntriesBegin));
    auto end = *reinterpret_cast<uint8_t**>(Field(aModManager, Addresses::CModManager_EntriesEnd));

    std::string list;
    for (auto entry = begin; entry < end; entry += Addresses::CModManager_EntrySize)
    {
        if (!(*(entry + Addresses::ModEntry_Enabled) & 1))
        {
            continue;
        }

        auto name = cstr(entry);
        list += fmt::format(" {}({})", name ? name : "?", *reinterpret_cast<int32_t*>(entry + Addresses::ModEntry_Order));
    }

    spdlog::info("Feral mod manager: enabled mods in load order:{}", list.empty() ? " none" : list);
}

uint64_t LinkPacks(void* aModManager)
{
    if (*Refreshing(aModManager))
    {
        // Feral's own wait (1 s) is over, Steam's callbacks keep coming in on its refresh thread
        auto start = std::chrono::steady_clock::now();
        while (*Refreshing(aModManager) && std::chrono::steady_clock::now() - start < MaxWait)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        if (*Refreshing(aModManager))
        {
            spdlog::warn("Feral mod manager: Steam's Workshop item details still missing after {} ms more, Workshop mods may be missing",
                         waited.count());
        }
        else
        {
            spdlog::info("Feral mod manager: waited {} ms more for Steam's Workshop item details", waited.count());
        }
    }

    reinterpret_cast<SortByOrder_t>(Image::Get()->Resolve(Addresses::CModManager_SortByOrder))(aModManager);
    LogEnabled(aModManager);

    return LinkPacks_fnc(aModManager);
}
} // namespace

bool Hooks::FeralModOrderHook::Attach()
{
    spdlog::trace("Trying to attach the Feral mod order hook at {:#x}...", LinkPacks_fnc.GetAddress());

    auto result = LinkPacks_fnc.Attach();
    if (result != 0)
    {
        spdlog::error("Could not attach the Feral mod order hook. Dobby error code: {}", result);
        return false;
    }

    spdlog::info("The Feral mod order hook was attached");
    return true;
}

bool Hooks::FeralModOrderHook::Detach()
{
    auto result = LinkPacks_fnc.Detach();
    if (result != 0)
    {
        spdlog::error("Could not detach the Feral mod order hook. Dobby error code: {}", result);
        return false;
    }

    spdlog::trace("The Feral mod order hook was detached");
    return true;
}

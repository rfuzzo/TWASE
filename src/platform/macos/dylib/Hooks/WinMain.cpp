#include "stdafx.hpp"

#include "WinMain.hpp"

#include "../Hooking/Hook.hpp"
#include "../Utils.hpp"

#include "../../sdk/Attila/Addresses.hpp"

namespace
{
int64_t WinMain(void* aInstance, void* aPrevInstance, const char16_t* aCommandLine);
Hook<decltype(&WinMain)> WinMain_fnc(sdk::Attila::Addresses::WinMain, &WinMain);

void LogMods(std::string_view aCommandLine)
{
    // statements are separated by ';', e.g. "mod @Fireforged-Empire_1.pack;"
    std::vector<std::string_view> mods;
    size_t start = 0;
    while (start < aCommandLine.size())
    {
        auto end = aCommandLine.find(';', start);
        if (end == std::string_view::npos)
        {
            end = aCommandLine.size();
        }

        auto statement = aCommandLine.substr(start, end - start);
        while (!statement.empty() && statement.front() == ' ')
        {
            statement.remove_prefix(1);
        }

        if (statement.starts_with("mod "))
        {
            mods.push_back(statement.substr(4));
        }

        start = end + 1;
    }

    if (mods.empty())
    {
        spdlog::info("No mods are enabled");
        return;
    }

    spdlog::info("Mods enabled ({}):", mods.size());
    for (auto mod : mods)
    {
        spdlog::info("  - {}", mod);
    }
}

int64_t WinMain(void* aInstance, void* aPrevInstance, const char16_t* aCommandLine)
{
    auto commandLine = Utils::ToUtf8(aCommandLine);
    spdlog::info("Game command line: {}", commandLine);
    LogMods(commandLine);

    return WinMain_fnc(aInstance, aPrevInstance, aCommandLine);
}
} // namespace

bool Hooks::WinMainHook::Attach()
{
    spdlog::trace("Trying to attach the WinMain hook at {:#x}...", WinMain_fnc.GetAddress());

    auto result = WinMain_fnc.Attach();
    if (result != 0)
    {
        spdlog::error("Could not attach the WinMain hook. Dobby error code: {}", result);
        return false;
    }

    spdlog::info("The WinMain hook was attached");
    return true;
}

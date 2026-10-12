#include "stdafx.hpp"

#include "SetLuaLogger.hpp"

#include "../App.hpp"
#include "../Config.hpp"
#include "../Image.hpp"
#include "../Hooking/Memory.hpp"
#include "../UI/LuaConsole.hpp"

#include "../../sdk/Attila/Addresses.hpp"

// The game's Lua output (print, out, the Lua error handler) goes through a global sink function pointer, see
// docs/addresses.md. The retail sink is an empty function, and every ScriptingEnv constructor sets it again.
// We nop the store in the setter so the game can't replace our sink, and install our own sink instead.
//
// The sink gets fragments (print sends each argument, the tabs and the final newline separately), so we collect them
// into lines.

namespace
{
using LuaLogSink_t = void (*)(const char* aText);

bool isAttached = false;
LuaLogSink_t originalSink = nullptr;

std::mutex lineMutex;
std::string line;

void ScriptLog(const char* aText)
{
    if (aText && App::Get() && App::Get()->GetConfig()->GetScripting().enableLogging)
    {
        std::scoped_lock lock(lineMutex);

        line += aText;

        size_t pos;
        while ((pos = line.find('\n')) != std::string::npos)
        {
            // don't log empty lines
            if (pos > 0)
            {
                auto text = std::string_view(line).substr(0, pos);
                spdlog::debug("[Lua] {}", text);

                // also echo into the ImGui console
                LuaConsole::Get().AddLog(LuaConsole::LogLevel::Info, "%.*s", static_cast<int>(text.size()), text.data());
            }

            line.erase(0, pos + 1);
        }
    }

    // keep whatever the game had installed before us working
    if (originalSink)
    {
        originalSink(aText);
    }
}
} // namespace

bool Hooks::LuaLogHook::Attach()
{
    using namespace sdk::Attila;

    spdlog::trace("Trying to attach the Lua log sink...");

    // SetLuaLogSink: str x0, [x8, #0x588] -> nop
    constexpr uint8_t expected[] = {0x00, 0xC5, 0x02, 0xF9};
    constexpr uint8_t patch[] = {0x1F, 0x20, 0x03, 0xD5};

    if (!Memory::PatchBytes("Lua log sink setter patch", Addresses::SetLuaLogSink_Store, expected, patch))
    {
        spdlog::error("Could not attach the Lua log sink");
        return false;
    }

    // __DATA, writable
    auto sink = Image::Get()->Resolve<LuaLogSink_t>(Addresses::g_LuaLogSink);
    originalSink = *sink;
    *sink = &ScriptLog;

    spdlog::info("The Lua log sink was attached (previous sink: {})", reinterpret_cast<void*>(originalSink));

    isAttached = true;
    return true;
}

bool Hooks::LuaLogHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }

    auto sink = Image::Get()->Resolve<LuaLogSink_t>(sdk::Attila::Addresses::g_LuaLogSink);
    *sink = originalSink;

    std::scoped_lock lock(lineMutex);
    if (!line.empty())
    {
        spdlog::debug("[Lua] {}", line);
        line.clear();
    }

    isAttached = false;
    return true;
}

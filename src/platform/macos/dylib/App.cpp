#include "stdafx.hpp"

#include "App.hpp"
#include "Integrity.hpp"
#include "TamperResponse.hpp"
#include "Image.hpp"
#include "Utils.hpp"
#include "Version.hpp"
#include "Patches/Patches.hpp"
#include "Hooking/Memory.hpp"

#include "Hooks/DiplomacyLikelihood.hpp"
#include "Hooks/FeralModOrder.hpp"
#include "Hooks/NativeRegionTrading.hpp"
#include "Hooks/GameTick.hpp"
#include "Hooks/LuaLoadTrace.hpp"
#include "Hooks/MetalHook.hpp"
#include "Hooks/RunStartupPath.hpp"
#include "Hooks/WinMain.hpp"
#include "Hooks/SetLuaLogger.hpp"

#include "../sdk/Attila/Addresses.hpp"
#include "../sdk/Attila/Lua/LuaGameEnvironment.hpp"
#include "../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../sdk/Attila/VFS/VFS.hpp"

#include <dispatch/dispatch.h>
#include <crt_externs.h>
#include <libproc.h>
#include <unistd.h>
#include <thread>

namespace
{
std::unique_ptr<App> g_app;
}

App::App()
    : m_config(m_paths)
{
    if (m_config.GetDev().waitForDebugger)
    {
        while (!Utils::IsDebuggerPresent())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    const auto filename = fmt::format("twase-{}.log", Utils::FormatCurrentTimestamp());

    auto logger = Utils::CreateLogger("TWASE", filename, m_paths, m_config);
    spdlog::set_default_logger(logger);

    spdlog::info("TWASE (v{}) is initializing...", TWASE_VERSION_STR);

    spdlog::debug("Using the following paths:");
    spdlog::debug("  Root: {}", m_paths.GetRootDir().string());
    spdlog::debug("  TWASE: {}", m_paths.GetTWASEDir().string());
    spdlog::debug("  Logs: {}", m_paths.GetLogsDir().string());
    spdlog::debug("  Config: {}", m_paths.GetConfigFile().string());

    spdlog::debug("Using the following configuration:");
    spdlog::debug("  version: {}", m_config.GetVersion());

    const auto& dev = m_config.GetDev();
    spdlog::debug("  dev.console: {}", dev.hasConsole);
    spdlog::debug("  dev.trace_lua_loads: {}", dev.traceLuaLoads);
    spdlog::debug("  dev.lua_dev_functions: {}", dev.luaDevFunctions);
    spdlog::debug("  dev.run_tamper_guards: {}", dev.runTamperGuards);
    spdlog::debug("  dev.watch_tamper_state: {}", dev.watchTamperState);
    spdlog::debug("  dev.reset_input_clock: {}", dev.resetInputClock);
    spdlog::debug("  dev.disarm_tamper_responses: {}", dev.disarmTamperResponses);
    spdlog::debug("  dev.late_patches: {}", dev.latePatches);
    spdlog::debug("  dev.input_clock_offset: {}", dev.inputClockOffset);

    const auto& loggingConfig = m_config.GetLogging();
    spdlog::debug("  logging.level: {}", spdlog::level::to_string_view(loggingConfig.level));
    spdlog::debug("  logging.flush_on: {}", spdlog::level::to_string_view(loggingConfig.flushOn));
    spdlog::debug("  logging.max_files: {}", loggingConfig.maxFiles);
    spdlog::debug("  logging.max_file_size: {} MB", loggingConfig.maxFileSize);

    const auto& pluginsConfig = m_config.GetPlugins();
    spdlog::debug("  plugins.enabled: {}", pluginsConfig.isEnabled);

    const auto& scriptingConfig = m_config.GetScripting();
    spdlog::debug("  scripting.enable_logging: {}", scriptingConfig.enableLogging);
    spdlog::debug("  scripting.auto_load_mods: {}", scriptingConfig.autoLoadMods);

    const auto& tweaksConfig = std::as_const(m_config).GetTweaks();
    spdlog::debug("  tweaks.diplomacy_deal_score: {}", tweaksConfig.diplomacyDealScore);

    const auto image = Image::Get();
    spdlog::info("Game version: {} (build {})", image->GetVersion(), image->GetBuild());
    spdlog::info("Executable UUID: {}", image->GetUUID());
    spdlog::debug("ASLR slide: {:#x}", image->GetSlide());

    // who started us (twase-launch.command, Steam, ...)
    const auto parent = getppid();
    char parentPath[PROC_PIDPATHINFO_MAXSIZE] = {};
    proc_pidpath(parent, parentPath, sizeof(parentPath));
    spdlog::info("Process {} started by {} ({})", getpid(), parent, parentPath[0] ? parentPath : "unknown");

    // Display commandline arguments
    auto argc = *_NSGetArgc();
    auto argv = *_NSGetArgv();
    spdlog::debug("Commandline arguments ({}):", argc);
    for (int i = 0; i < argc; ++i)
    {
        spdlog::debug("  [{}]: {}", i, argv[i]);
    }

}

// separate from the constructor, hooks may use App::Get() which is only set after construction
void App::Initialize()
{
    if (!Image::Get()->IsSupported())
    {
        spdlog::error("This game build is not supported (expected UUID {}), TWASE will not hook anything",
                      sdk::Attila::Addresses::SupportedUUID);
        return;
    }

    if (!Integrity::InstallCleanTextAnchor())
    {
        spdlog::error("Clean text anchor could not be installed, TWASE will not apply patches or hooks");
        return;
    }

    Integrity::LogStatus("before patches and hooks");

    // We run before main(), the game code is already mapped, so everything can be applied right away. With [dev]
    // late_patches the byte patches wait for the game's run loop instead, after its static initializers, which hash the
    // whole __text and record changes (docs/macos/integrity-and-quirks.md)
    if (IsEnabled("unit_size_patch"))
    {
        if (m_config.GetDev().latePatches)
        {
            spdlog::info("Byte patches deferred to the game's run loop ([dev] late_patches)");
            dispatch_async_f(dispatch_get_main_queue(), nullptr,
                             [](void*)
                             {
                                 Patches::ApplyPatches();
                                 Integrity::LogStatus("after unit_size_patch (late)");
                             });
        }
        else
        {
            Patches::ApplyPatches();
            Integrity::LogStatus("after unit_size_patch");
        }
    }

    // [dev] remap_identical: remap the unit_size_patch page with its own bytes (no content change), to tell whether the
    // game notices the remapped page or the changed bytes (docs/macos/integrity-and-quirks.md)
    if (m_config.GetDev().remapIdentical)
    {
        const auto address = Image::Get()->Resolve(sdk::Attila::Addresses::BitSetCrashAddr);
        uint8_t same[4];
        std::memcpy(same, reinterpret_cast<const void*>(address), sizeof(same));
        spdlog::info("remap_identical: remapped the page at {:#x} with unchanged bytes: {}",
                     sdk::Attila::Addresses::BitSetCrashAddr, Memory::WriteCode(address, same, sizeof(same)));
    }

    if (m_config.GetDev().disarmTamperResponses)
    {
        TamperResponse::DisarmResponses();
    }

    if (AttachHooks())
    {
        spdlog::info("TWASE has been successfully initialized");
    }
    else
    {
        spdlog::error("TWASE did not initialize properly");
    }

    Integrity::StartPeriodicCheck();
}

bool App::IsEnabled(const std::string& aName) const
{
    if (m_config.GetDev().IsDisabled(aName))
    {
        spdlog::warn("Skipping {} ([dev] disable in config.ini)", aName);
        return false;
    }
    return true;
}

void App::Construct()
{
    g_app.reset(new App());
    g_app->Initialize();
}

void App::Destruct()
{
    spdlog::info("TWASE is terminating...");

    // Hooks are not detached here, the process is exiting and other threads may still run game code.
    Hooks::LuaLogHook::Detach();

    g_app.reset(nullptr);
    spdlog::info("TWASE has been terminated");

    spdlog::details::registry::instance().flush_all();
    spdlog::shutdown();
}

App* App::Get()
{
    return g_app.get();
}

const Paths* App::GetPaths() const
{
    return &m_paths;
}

const Config* App::GetConfig() const
{
    return &m_config;
}

Config* App::GetConfig()
{
    return &m_config;
}

bool App::AttachHooks()
{
    spdlog::info("Attaching hooks...");

    // Resolve Lua function pointers (non-hooked, called directly)
    const auto slide = Image::Get()->GetSlide();
    LuaRuntime::Init(slide);
    LuaGameEnvironment::Init(slide);
    VFS::Init(slide);

    // each patch / hook by name, so it can be skipped with [dev] disable (to find what Feral's integrity checks notice)
    auto success = true;
    const auto attach = [this, &success](const char* aName, bool (*aAttach)())
    {
        if (IsEnabled(aName))
        {
            success &= aAttach();
            Integrity::LogStatus(fmt::format("after {}", aName));
        }
    };

    attach("lua_log", &Hooks::LuaLogHook::Attach); // a byte patch plus the game's log sink

    // developer option, an extra hook on every Lua chunk load
    if (m_config.GetDev().traceLuaLoads)
    {
        attach("lua_load_trace", &Hooks::LuaLoadTraceHook::Attach);
    }
    attach("game_tick", &Hooks::GameTickHook::Attach);
    attach("win_main", &Hooks::WinMainHook::Attach);
    attach("mod_loader", &Hooks::RunStartupPathHook::Attach);
    attach("diplomacy", &Hooks::DiplomacyLikelihoodHook::Attach);
    attach("region_trading", &Hooks::NativeRegionTradingHook::Attach);
    attach("feral_mod_order", &Hooks::FeralModOrderHook::Attach);

    // the console is optional, TWASE still works without it (Objective-C method swizzling, no game code changed)
    if (IsEnabled("overlay"))
    {
        if (!Hooks::MetalHook::Attach())
        {
            spdlog::warn("Metal hook failed – Lua console will not be available");
        }
        Integrity::LogStatus("after overlay");
    }

    return success;
}

#include "stdafx.hpp"

#include "App.hpp"
#include "Image.hpp"
#include "Utils.hpp"
#include "Version.hpp"
#include "Patches.hpp"

#include <shellapi.h>

namespace
{
std::unique_ptr<App> g_app;
}

namespace {
    DWORD WINAPI WatcherThread(LPVOID)
    {
        // Wait for the module to be loaded in-process (poll)
        HMODULE hMod = nullptr;
        while (!hMod) {
            hMod = GetModuleHandle(L"empire.retail.dll");
            if (!hMod) Sleep(25); // small sleep, doesn't block loader
        }

        MODULEINFO mi = { 0 };
        if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) {
            // handle error
            return 1;
        }

        const DWORD empireDllAddr = reinterpret_cast<DWORD>(mi.lpBaseOfDll);
        spdlog::debug("empire.retail.dll lpBaseOfDll address: {}", reinterpret_cast<void*>(mi.lpBaseOfDll));
        
        Patches::ApplyEmpirePatches(empireDllAddr);

        return 0;
    }
}

App::App()
    : m_config(m_paths)
    , m_devConsole(m_config.GetDev())
{
    if (m_config.GetDev().waitForDebugger)
    {
        while (!IsDebuggerPresent())
        {
            std::this_thread::yield();
        }
    }

    const auto filename = fmt::format(L"twase-{}.log", Utils::FormatCurrentTimestamp());

    auto logger = Utils::CreateLogger(L"TWASE", filename, m_paths, m_config, m_devConsole);
    spdlog::set_default_logger(logger);

    spdlog::info("TWASE (v{}) is initializing...", TWASE_VERSION_STR);

    spdlog::debug("Using the following paths:");
    spdlog::debug("  Root: {}", Utils::Narrow(m_paths.GetRootDir().c_str()));
    spdlog::debug("  TWASE: {}", Utils::Narrow(m_paths.GetTWASEDir().c_str()));
    spdlog::debug("  Logs: {}", Utils::Narrow(m_paths.GetLogsDir().c_str()));
    spdlog::debug("  Config: {}", Utils::Narrow(m_paths.GetConfigFile().c_str()));
    //spdlog::debug("  Plugins: {}", m_paths.GetPluginsDir());

    spdlog::debug("Using the following configuration:");
    spdlog::debug("  version: {}", m_config.GetVersion());

    const auto& dev = m_config.GetDev();
    spdlog::debug("  dev.console: {}", dev.hasConsole);

    const auto& loggingConfig = m_config.GetLogging();
    spdlog::debug("  logging.level: {}", spdlog::level::to_string_view(loggingConfig.level));
    spdlog::debug("  logging.flush_on: {}", spdlog::level::to_string_view(loggingConfig.flushOn));
    spdlog::debug("  logging.max_files: {}", loggingConfig.maxFiles);
    spdlog::debug("  logging.max_file_size: {} MB", loggingConfig.maxFileSize);

    const auto& pluginsConfig = m_config.GetPlugins();
    spdlog::debug("  plugins.enabled: {}", pluginsConfig.isEnabled);

   /* const auto& ignored = pluginsConfig.ignored;
    if (ignored.empty())
    {
        spdlog::debug("  plugins.ignored: []");
    }
    else
    {
        spdlog::debug("  plugins.ignored: [ {} ]", fmt::join(ignored, L", "));
    }*/

    spdlog::debug("Base address is: {}", reinterpret_cast<void*>(GetModuleHandle(nullptr)));

    const auto image = Image::Get();
    const auto& fileVer = image->GetFileVersion();

    const auto& productVer = image->GetProductVersion();
    spdlog::info("Product version: {}.{}{}", productVer.major, productVer.minor, productVer.patch);
    spdlog::info("File version: {}.{}.{}.{}", fileVer.major, fileVer.minor, fileVer.build, fileVer.revision);

	// TODO: Check for minimum supported version.
   /* auto minimumVersion = RED4EXT_RUNTIME_2_31;
    if (fileVer < RED4EXT_RUNTIME_2_31)
    {
        spdlog::error(L"To use this version of RED4ext, ensure your game is updated to patch 2.31 or newer");
        return;
    }*/

	// Display commandline arguments
    bool hasModsEnabled = false;
	int argc = 0;
	wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
	spdlog::debug("Commandline arguments ({}):", argc);
	for (int i = 0; i < argc; ++i)
	{
		std::string arg = Utils::Narrow(argv[i]);
		spdlog::debug("  [{}]: {}", i, arg);
        if (arg == "mod_list.txt;")
        {
            hasModsEnabled = true;
        }
	}

	// Display mods if the commandline argument is present "mod_list.txt"
    if (hasModsEnabled)
    {
        LogMods();
    }

    // Create a detached watcher thread; returns immediately and does not block loader
    HANDLE h = CreateThread(nullptr, 0, WatcherThread, nullptr, 0, nullptr);
    if (h)
    { 
        CloseHandle(h); 
    }

    // Finalize
    if (AttachHooks())
    {
        spdlog::info("TWASE has been successfully initialized");
    }
    else
    {
        spdlog::error("TWASE did not initialize properly");
    }
}

void App::Construct()
{
    g_app.reset(new App());
}

void App::Destruct()
{
    spdlog::info("TWASE is terminating...");

    // Detaching hooks here and not in dtor, since the dtor can be called by CRT when the processes exists. We don't
    // really care if this will be called or not when the game exist ungracefully.

    spdlog::trace("Detaching the hooks...");

    g_app.reset(nullptr);
    spdlog::info("TWASE has been terminated");

    spdlog::details::registry::instance().flush_all();
    spdlog::shutdown();
}

App* App::Get()
{
    return g_app.get();
}

void App::Startup()
{
    spdlog::info("TWASE is starting up...");

    // TODO

    spdlog::info("TWASE has been started");
}

void App::Shutdown()
{
    spdlog::info("TWASE is shutting down...");

    // TODO

    // Flushing the log here, since it is called in the main function, not when DLL is unloaded.
    spdlog::details::registry::instance().flush_all();
}

const Paths* App::GetPaths() const
{
    return &m_paths;
}

bool App::AttachHooks() const
{
    spdlog::trace("Attaching hooks...");

    // TODO

    return true;
}

void App::LogMods() const
{
    // Load and display mods from the file
    const std::filesystem::path modListPath = m_paths.GetRootDir() / "mod_list.txt";
    std::ifstream modListFile(modListPath);
    if (modListFile.is_open())
    {
        spdlog::info("Mods loaded from mod_list.txt:");
        std::string modName;
        while (std::getline(modListFile, modName))
        {
            // mod "@Fireforged-Empire_1.pack";
            // check if line starts with 'mod "'
            if (modName.rfind("mod ", 0) == 0)
            {
                spdlog::info("  - {}", modName);
            }
        }
        modListFile.close();
    }
    else
    {
        spdlog::warn("Could not open mod_list.txt to read mods.");
    }
}
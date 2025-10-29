#include "App.hpp"
#include "Image.hpp"
#include "Utils.hpp"
#include "Version.hpp"
#include "stdafx.hpp"

namespace
{
std::unique_ptr<App> g_app;
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

   

    spdlog::info("TWASE has been started");
}

void App::Shutdown()
{
    spdlog::info("TWASE is shutting down...");



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

    return true;
}

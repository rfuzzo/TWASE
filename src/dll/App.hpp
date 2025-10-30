#pragma once

#include "Paths.hpp"
#include "Config.hpp"
#include "DevConsole.hpp"

class App
{
public:
    ~App() = default;

    static void Construct();
    static void Destruct();
    static App* Get();

    void Startup();
    void Shutdown();

    const Paths* GetPaths() const;

private:
    App();

    bool AttachHooks() const;
    void LogMods() const;

    Paths m_paths;
    Config m_config;
    DevConsole m_devConsole;
};

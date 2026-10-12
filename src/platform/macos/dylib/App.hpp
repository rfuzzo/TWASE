#pragma once

#include "Config.hpp"
#include "Paths.hpp"

class App
{
public:
    ~App() = default;

    static void Construct();
    static void Destruct();
    static App* Get();

    const Paths* GetPaths() const;
    const Config* GetConfig() const;
    Config* GetConfig();

private:
    App();

    void Initialize();

    bool AttachHooks();

    Paths m_paths;
    Config m_config;
};

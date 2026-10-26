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

    // false (and logged) for a patch or hook listed in [dev] disable
    bool IsEnabled(const std::string& aName) const;

    Paths m_paths;
    Config m_config;
};

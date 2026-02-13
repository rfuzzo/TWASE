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
	const Config* GetConfig() const;
	inline const DWORD GetEmpireDllAddr() const { return m_empireDllAddr; }

    bool AttachHooks(DWORD empireDllAddr);

private:
    App();

    void LogMods() const;

    Paths m_paths;
    Config m_config;
    DevConsole m_devConsole;

	// TOOD move into subsystem
	DWORD m_empireDllAddr;
};

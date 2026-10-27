#pragma once

#include "Paths.hpp"

class Config
{
public:
    static constexpr size_t LatestVersion = 0;
    static constexpr size_t MinSupportedVersion = 0;
    static constexpr size_t MaxSupportedVersion = 0;

    struct DevConfig
    {
        void LoadV0(const toml::value& aConfig);

        bool hasConsole = false;
        bool waitForDebugger = false;
        bool traceLuaLoads = false;
        bool luaDevFunctions = false;
        // Mac: call Feral's anti-tamper code guards directly and log the tamper counter (reverse engineering)
        bool runTamperGuards = false;
        // Mac: hardware watchpoints on the state of Feral's startup code guards, logging who reads it
        bool watchTamperState = false;
        // Mac: keep resetting Feral's hidden "seconds since the first input event" clock (quirk experiment)
        bool resetInputClock = false;
        // Mac: move Feral's hidden input clock this many seconds forward once, to reach the quirk quickly (0 = off)
        int32_t inputClockOffset = 0;
        // patches and hooks to skip, by the name in the log (Mac, for finding what Feral's integrity checks notice)
        std::unordered_set<std::string> disabled;

        bool IsDisabled(const std::string& aName) const { return disabled.contains(aName); }
    };

    struct ScriptConfig
    {
        void LoadV0(const toml::value& aConfig);

        bool enableLogging = true;
        bool autoLoadMods = true;
    };

    struct TweaksConfig
    {
        void LoadV0(const toml::value& aConfig);

        bool diplomacyDealScore = true;
    };

    struct LoggingConfig
    {
        void LoadV0(const toml::value& aConfig);

        spdlog::level::level_enum level = spdlog::level::info;
        spdlog::level::level_enum flushOn = spdlog::level::info;
        uint32_t maxFiles = 5;
        uint32_t maxFileSize = 10;
    };

    struct PluginsConfig
    {
        void LoadV0(const toml::value& aConfig);

        bool isEnabled = true;
        std::unordered_set<std::string> ignored;
    };

    Config(const Paths& aPaths);
    ~Config() = default;

    size_t GetVersion() const;

    const DevConfig& GetDev() const;
    const LoggingConfig& GetLogging() const;
    const PluginsConfig& GetPlugins() const;
    const ScriptConfig& GetScripting() const;
    const TweaksConfig& GetTweaks() const;
    TweaksConfig& GetTweaks();

    // Writes the current values to the config file, keeping the user's comments and layout. Returns false on error.
    bool Save();

private:
    void Load(const std::filesystem::path& aFile);
    bool Save(const std::filesystem::path& aFile, std::string& aError);

    void LoadV0(const toml::value& aConfig);

    std::filesystem::path m_file;
    size_t m_version;

    DevConfig m_dev;
    LoggingConfig m_logging;
    PluginsConfig m_plugins;
    ScriptConfig m_scripting;
    TweaksConfig m_tweaks;
};

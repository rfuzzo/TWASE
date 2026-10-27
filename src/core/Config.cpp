#include "stdafx.hpp"

#include "Config.hpp"
#include "Platform.hpp"

#define DEFAULT_TOML_EXCEPTION_MSG "An exception occured while parsing the config file:\n\n{}\n\nFile: {}"

Config::Config(const Paths& aPaths)
    : m_file(aPaths.GetConfigFile())
    , m_version(0)
    , m_dev()
    , m_logging()
    , m_plugins()
{
    const auto& file = m_file;

    std::error_code err;
    if (std::filesystem::exists(file, err))
    {
        Load(file);
    }
    else if (err)
    {
        TWASE_MESSAGE_BOX_AND_EXIT_FILE_LINE("An error occured while checking config file existence:\n{}\n\nFile: {}",
                                             err.message(), Platform::ToUtf8(file));
    }
    else
    {
        std::filesystem::create_directories(file.parent_path(), err);

        std::string error;
        if (!Save(file, error))
        {
            TWASE_MESSAGE_BOX_FILE_LINE("An exception occured while saving the config file:\n\n{}\n\nFile: {}", error,
                                        Platform::ToUtf8(file));
        }
    }
}

size_t Config::GetVersion() const
{
    return m_version;
}

const Config::DevConfig& Config::GetDev() const
{
    return m_dev;
}

const Config::LoggingConfig& Config::GetLogging() const
{
    return m_logging;
}

const Config::PluginsConfig& Config::GetPlugins() const
{
    return m_plugins;
}

const Config::ScriptConfig& Config::GetScripting() const
{
    return m_scripting;
}

const Config::TweaksConfig& Config::GetTweaks() const
{
    return m_tweaks;
}

Config::TweaksConfig& Config::GetTweaks()
{
    return m_tweaks;
}

void Config::Load(const std::filesystem::path& aFile)
{
    try
    {
        auto config = toml::parse(aFile);
        if (config.contains("version"))
        {
            auto version = toml::find<size_t>(config, "version");
            switch (version)
            {
            case 0:
            {
                LoadV0(config);
                break;
            }
            default:
            {
                TWASE_MESSAGE_BOX_AND_EXIT_FILE_LINE(
                    "Config version '{}' is not supported (version < {} or version > {}).\n\nFile: {}", version,
                    MinSupportedVersion, MaxSupportedVersion, Platform::ToUtf8(aFile));
                break;
            }
            }
        }
        else
        {
            TWASE_MESSAGE_BOX_AND_EXIT_FILE_LINE("The config file does not have a version.\n\nFile: {}",
                                                 Platform::ToUtf8(aFile));
        }
    }
    catch (const std::exception& e)
    {
        TWASE_MESSAGE_BOX_AND_EXIT_FILE_LINE(DEFAULT_TOML_EXCEPTION_MSG, e.what(), Platform::ToUtf8(aFile));
    }
}

namespace
{
toml::ordered_value& GetSection(toml::ordered_value& aConfig, const std::string& aName)
{
    auto& section = aConfig[aName];
    if (!section.is_table())
    {
        section = toml::ordered_table{};
    }

    return section;
}

// sets the value in place so existing comments and formatting are kept, new keys get the comment
template<typename T>
void SetValue(toml::ordered_value& aTable, const std::string& aKey, const T& aValue, const char* aComment = nullptr)
{
    const auto isNew = !aTable.contains(aKey);

    auto& value = aTable[aKey];
    value = aValue;

    if (isNew && aComment)
    {
        value.comments().push_back(std::string(" ") + aComment);
    }
}

std::string LevelToString(spdlog::level::level_enum aLevel)
{
    auto name = spdlog::level::to_string_view(aLevel);
    return std::string(name.data(), name.size());
}
} // namespace

bool Config::Save()
{
    std::string error;
    if (!Save(m_file, error))
    {
        spdlog::error("Could not save the config file '{}': {}", Platform::ToUtf8(m_file), error);
        return false;
    }

    spdlog::info("Saved the config file '{}'", Platform::ToUtf8(m_file));
    return true;
}

bool Config::Save(const std::filesystem::path& aFile, std::string& aError)
{
    try
    {
        // update the existing file so the user's comments and layout are kept
        toml::ordered_value config(toml::ordered_table{});

        std::error_code err;
        if (std::filesystem::exists(aFile, err))
        {
            config = toml::parse<toml::ordered_type_config>(aFile);
        }
        else
        {
            config.comments().push_back(" TWASE configuration, see the README for all options.");
        }

        SetValue(config, "version", static_cast<std::int64_t>(LatestVersion));

        auto& logging = GetSection(config, "logging");
        SetValue(logging, "level", LevelToString(m_logging.level), "trace, debug, info, warn, err, critical, off");
        SetValue(logging, "flush_on", LevelToString(m_logging.flushOn));
        SetValue(logging, "max_files", static_cast<std::int64_t>(m_logging.maxFiles));
        SetValue(logging, "max_file_size", static_cast<std::int64_t>(m_logging.maxFileSize), "MB");

        auto& scripting = GetSection(config, "scripting");
        SetValue(scripting, "enable_logging", m_scripting.enableLogging,
                 "Forward game Lua log output to the console and log files");
        SetValue(scripting, "auto_load_mods", m_scripting.autoLoadMods,
                 "Auto-load mods from <campaign_folder>/mods/*/scripting.lua");

        auto& tweaks = GetSection(config, "tweaks");
        SetValue(tweaks, "diplomacy_deal_score", m_tweaks.diplomacyDealScore,
                 "Show the AI deal score in the diplomacy likelihood tooltip");

        auto& plugins = GetSection(config, "plugins");
        SetValue(plugins, "enabled", m_plugins.isEnabled);
        if (!plugins.contains("ignored"))
        {
            // the ignored list is never changed at runtime, keep whatever the file has
            plugins["ignored"] = toml::ordered_array{};
        }

        auto& dev = GetSection(config, "dev");
        SetValue(dev, "console", m_dev.hasConsole, "Mirror the log to stdout (visible when started from a terminal)");
        SetValue(dev, "wait_for_debugger", m_dev.waitForDebugger);
        SetValue(dev, "trace_lua_loads", m_dev.traceLuaLoads,
                 "Log every Lua chunk the game loads (needs logging.level = \"trace\")");
        SetValue(dev, "lua_dev_functions", m_dev.luaDevFunctions,
                 "Add the twase.dev_* probes for reverse engineering to the Lua API");
        SetValue(dev, "run_tamper_guards", m_dev.runTamperGuards,
                 "Mac: call Feral's anti-tamper code guards 30 s after start and log the tamper counter");
        SetValue(dev, "watch_tamper_state", m_dev.watchTamperState,
                 "Mac: hardware watchpoints on Feral's anti-tamper state, logging the code that touches it");
        SetValue(dev, "reset_input_clock", m_dev.resetInputClock,
                 "Mac: keep resetting Feral's hidden input clock (experiment for the click quirk)");
        SetValue(dev, "input_clock_offset", m_dev.inputClockOffset,
                 "Mac: move Feral's hidden input clock this many seconds forward once (0 = off)");
        if (!dev.contains("disable"))
        {
            // never changed at runtime, keep whatever the file has
            dev["disable"] = toml::ordered_array{};
            dev["disable"].comments().push_back(
                " Mac: patches and hooks to skip, e.g. [\"unit_size_patch\", \"region_trading\"] (names in the log)");
        }

        // write to a temporary file first so a failed write doesn't destroy the config
        auto tempFile = aFile;
        tempFile += ".tmp";
        {
            std::ofstream file(tempFile, std::ios::out | std::ios::trunc);
            file.exceptions(std::ostream::badbit | std::ostream::failbit);
            file << toml::format(config);
        }

        std::filesystem::rename(tempFile, aFile);
        return true;
    }
    catch (const std::exception& e)
    {
        aError = e.what();
        return false;
    }
}

void Config::LoadV0(const toml::value& aConfig)
{
    m_version = 0;

    m_dev.LoadV0(aConfig);
    m_logging.LoadV0(aConfig);
    m_plugins.LoadV0(aConfig);
    m_scripting.LoadV0(aConfig);
    m_tweaks.LoadV0(aConfig);
}

void Config::DevConfig::LoadV0(const toml::value& aConfig)
{
    hasConsole = toml::find_or(aConfig, "dev", "console", hasConsole);
    waitForDebugger = toml::find_or(aConfig, "dev", "wait_for_debugger", waitForDebugger);
    traceLuaLoads = toml::find_or(aConfig, "dev", "trace_lua_loads", traceLuaLoads);
    luaDevFunctions = toml::find_or(aConfig, "dev", "lua_dev_functions", luaDevFunctions);
    runTamperGuards = toml::find_or(aConfig, "dev", "run_tamper_guards", runTamperGuards);
    watchTamperState = toml::find_or(aConfig, "dev", "watch_tamper_state", watchTamperState);
    resetInputClock = toml::find_or(aConfig, "dev", "reset_input_clock", resetInputClock);
    inputClockOffset = toml::find_or(aConfig, "dev", "input_clock_offset", inputClockOffset);

    std::vector<std::string> disabledNames;
    disabledNames = toml::find_or(aConfig, "dev", "disable", disabledNames);
    disabled.insert(disabledNames.begin(), disabledNames.end());
}

void Config::ScriptConfig::LoadV0(const toml::value& aConfig)
{
    enableLogging = toml::find_or(aConfig, "scripting", "enable_logging", enableLogging);
    autoLoadMods = toml::find_or(aConfig, "scripting", "auto_load_mods", autoLoadMods);
}

void Config::TweaksConfig::LoadV0(const toml::value& aConfig)
{
    diplomacyDealScore = toml::find_or(aConfig, "tweaks", "diplomacy_deal_score", diplomacyDealScore);
}

void Config::LoggingConfig::LoadV0(const toml::value& aConfig)
{
    auto levelName = toml::find_or(aConfig, "logging", "level", "");
    if (!levelName.empty())
    {
        auto requestedLevel = spdlog::level::from_str(levelName);

        // If the level is set to off, but the requested level is not "off" then the user might mistyped the levels.
        // spdlog return "level::off" if there is no match.
        if (requestedLevel == spdlog::level::off && levelName != "off")
        {
            requestedLevel = level;
        }

        level = requestedLevel;
    }

    levelName = toml::find_or(aConfig, "logging", "flush_on", "");
    if (!levelName.empty())
    {
        auto requestedLevel = spdlog::level::from_str(levelName.data());

        // Do not allow flushing to be off.
        if (requestedLevel == spdlog::level::off)
        {
            requestedLevel = flushOn;
        }

        flushOn = requestedLevel;
    }

    maxFiles = toml::find_or(aConfig, "logging", "max_files", maxFiles);
    if (maxFiles < 1)
    {
        maxFiles = 5;
    }

    maxFileSize = toml::find_or(aConfig, "logging", "max_file_size", maxFileSize);
    if (maxFileSize < 1)
    {
        maxFileSize = 10;
    }
}

void Config::PluginsConfig::LoadV0(const toml::value& aConfig)
{
    isEnabled = toml::find_or(aConfig, "plugins", "enabled", isEnabled);

    std::vector<std::string> ignoredPlugins;
    ignoredPlugins = toml::find_or(aConfig, "plugins", "ignored", ignoredPlugins);

    ignored.insert(ignoredPlugins.begin(), ignoredPlugins.end());
}

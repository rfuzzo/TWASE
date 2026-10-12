#pragma once

class Paths
{
public:
    Paths();
    ~Paths() = default;

    std::filesystem::path GetRootDir() const;

    std::filesystem::path GetExe() const;

    std::filesystem::path GetTWASEDir() const;
    std::filesystem::path GetLogsDir() const;
    std::filesystem::path GetPluginsDir() const;

    const std::filesystem::path GetConfigFile() const;

private:
    std::filesystem::path m_root;
    std::filesystem::path m_exe;
};

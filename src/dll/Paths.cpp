#include "Paths.hpp"
#include "Utils.hpp"

Paths::Paths()
{
    std::wstring fileName;
    auto hr = wil::GetModuleFileNameW(nullptr, fileName);
    if (FAILED(hr))
    {
        SHOW_LAST_ERROR_MESSAGE_AND_EXIT_FILE_LINE(L"Could not get game's file name.");
        return;
    }

    m_exe = fileName;
    m_root = m_exe.parent_path(); // Resolve to game's root directory.
}

std::filesystem::path Paths::GetRootDir() const
{
    return m_root;
}

std::filesystem::path Paths::GetExe() const
{
    return m_exe;
}

std::filesystem::path Paths::GetTWASEDir() const
{
    return GetRootDir() / L"TWASE";
}

std::filesystem::path Paths::GetLogsDir() const
{
    return GetTWASEDir() / L"logs";
}

std::filesystem::path Paths::GetPluginsDir() const
{
    return GetTWASEDir() / L"plugins";
}

const std::filesystem::path Paths::GetConfigFile() const
{
    return GetTWASEDir() / L"config.ini";
}

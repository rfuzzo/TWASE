#include "stdafx.hpp"

#include "Paths.hpp"
#include "Utils.hpp"
#include "Image.hpp"

#include <cstdlib>

#include <mach-o/dyld.h>

Paths::Paths()
{
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0)
    {
        SHOW_MESSAGE_BOX_AND_EXIT_FILE_LINE("Could not get the game's executable path.");
        return;
    }

    std::error_code err;
    m_exe = std::filesystem::canonical(buffer, err);
    if (err)
    {
        m_exe = buffer;
    }

    // <root>/Total War ATTILA.app/Contents/MacOS/Total War ATTILA
    m_root = m_exe.parent_path().parent_path().parent_path().parent_path();

    if (Image::Get()->IsAppStore())
    {
        // The App Store development launcher isolates writable state from the installed app and Steam.
        if (const auto* root = std::getenv("TWASE_ROOT"); root && *root)
        {
            m_root = std::filesystem::absolute(root);
        }
        else if (const auto* home = std::getenv("HOME"); home && *home)
        {
            m_root = std::filesystem::path(home) / "Library" / "Application Support" / "TWASE App Store";
        }
        else
        {
            SHOW_MESSAGE_BOX_AND_EXIT_FILE_LINE("App Store TWASE requires a writable home or TWASE_ROOT.");
        }
    }
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
    return GetRootDir() / "TWASE";
}

std::filesystem::path Paths::GetLogsDir() const
{
    return GetTWASEDir() / "logs";
}

std::filesystem::path Paths::GetPluginsDir() const
{
    return GetTWASEDir() / "plugins";
}

const std::filesystem::path Paths::GetConfigFile() const
{
    return GetTWASEDir() / "config.ini";
}

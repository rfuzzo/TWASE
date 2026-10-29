#pragma once

#include "../sdk/Attila/Build.hpp"

class Image
{
public:
    static Image* Get();

    bool IsAttila() const;
    // True only for a build with a complete, verified address table, false before SelectBuild.
    bool IsSupported() const;

    // Attila.exe version resource
    const FileVer& GetFileVersion() const;
    const SemVer& GetProductVersion() const;

    // Reads the loaded empire.retail.dll's build ID and selects its addresses, before any patch or hook.
    void SelectBuild(HMODULE aEmpireDll);
    // CodeView PDB GUID + age in symbol server form, the Windows counterpart of the Mac LC_UUID. Empty if not found.
    const std::string& GetBuildId() const;

private:
    Image();
    ~Image() = default;

    bool m_isAttila;
    FileVer m_fileVersion;
    SemVer m_productVersion;
    sdk::Attila::Build m_gameBuild = sdk::Attila::Build::Unknown;
    std::string m_buildId;
};

#pragma once

class Image
{
public:
    static Image* Get();

    bool IsAttila() const;

    // Attila.exe version resource
    const FileVer& GetFileVersion() const;
    const SemVer& GetProductVersion() const;

    // Build ID of a loaded module: the CodeView PDB GUID and age from its debug directory, in symbol server form
    // (32 hex digits GUID, then the age in hex). The Windows counterpart of the Mac LC_UUID. Empty if there is none.
    static std::string ReadBuildId(HMODULE aModule);

private:
    Image();
    ~Image() = default;

    bool m_isAttila;
    FileVer m_fileVersion;
    SemVer m_productVersion;
};

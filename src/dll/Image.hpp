#pragma once

class Image
{
public:
    static Image* Get();

    bool IsAttila() const;
    bool IsSupported() const;

    const FileVer& GetFileVersion() const;
    const SemVer& GetProductVersion() const;
    const std::vector<FileVer> GetSupportedVersions() const;

private:
    Image();
    ~Image() = default;

    bool m_isAttila;
    FileVer m_fileVersion;
    SemVer m_productVersion;
};

#pragma once

#include <sstream>

TWASE_INLINE FileVer CreateFileVer(uint16_t aMajor, uint16_t aMinor, uint16_t aBuild,
                                                               uint16_t aRevision)
{
    FileVer version{};
    version.major = aMajor;
    version.minor = aMinor;
    version.build = aBuild;
    version.revision = aRevision;

    return version;
}

TWASE_INLINE int32_t CompareFileVer(const FileVer& aLhs, const FileVer& aRhs)
{
    if (aLhs.major != aRhs.major)
    {
        return aLhs.major - aRhs.major;
    }
    else if (aLhs.minor != aRhs.minor)
    {
        return aLhs.minor - aRhs.minor;
    }
    else if (aLhs.build != aRhs.build)
    {
        return aLhs.build - aRhs.build;
    }
    else if (aLhs.revision != aRhs.revision)
    {
        return aLhs.revision - aRhs.revision;
    }

    return 0;
}

TWASE_INLINE std::wstring std::to_wstring(const FileVer& aVersion)
{
    std::wstringstream stream;
    stream << aVersion.major << L"." << aVersion.minor << L"." << aVersion.build << L"." << aVersion.revision;

    return stream.str();
}

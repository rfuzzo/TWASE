#pragma once

#include <sstream>

TWASE_INLINE SemVer CreateSemVer(uint8_t aMajor, uint16_t aMinor, uint32_t aPatch,
                                                             uint32_t aPrereleaseType, uint32_t aPrereleaseNumber)
{
    SemVer version{};
    version.major = aMajor;
    version.minor = aMinor;
    version.patch = aPatch;
    version.prerelease.type = aPrereleaseType;
    version.prerelease.number = aPrereleaseNumber;

    return version;
}

TWASE_INLINE int32_t CompareSemVer(const SemVer& aLhs, const SemVer& aRhs)
{
    if (aLhs.major != aRhs.major)
    {
        return aLhs.major - aRhs.major;
    }
    else if (aLhs.minor != aRhs.minor)
    {
        return aLhs.minor - aRhs.minor;
    }
    else if (aLhs.patch != aRhs.patch)
    {
        return aLhs.patch - aRhs.patch;
    }

    return CompareSemVerPrerelease(aLhs.prerelease, aRhs.prerelease);
}

TWASE_INLINE int32_t CompareSemVerPrerelease(const SemVer::PrereleaseInfo& aLhs,
                                                            const SemVer::PrereleaseInfo& aRhs)
{
    if (aLhs.type != aRhs.type)
    {
        return aLhs.type - aRhs.type;
    }
    else if (aLhs.number != aRhs.number)
    {
        return aLhs.number - aRhs.number;
    }

    return 0;
}

TWASE_INLINE std::wstring std::to_wstring(const SemVer& aVersion)
{
    std::wstringstream stream;
    stream << aVersion.major << L"." << aVersion.minor << L"." << aVersion.patch;

    if (aVersion.prerelease.type != TWASE_SEMVER_PRERELEASE_TYPE_NONE)
    {
        stream << L"-";
        switch (aVersion.prerelease.type)
        {
        case TWASE_SEMVER_PRERELEASE_TYPE_ALPHA:
        {
            stream << L"alpha";
            break;
        }
        case TWASE_SEMVER_PRERELEASE_TYPE_BETA:
        {
            stream << L"beta";
            break;
        }
        case TWASE_SEMVER_PRERELEASE_TYPE_RC:
        {
            stream << L"rc";
            break;
        }
        default:
        {
            stream << L"unknown";
            break;
        }
        }

        stream << L"." << aVersion.prerelease.number;
    }

    return stream.str();
}

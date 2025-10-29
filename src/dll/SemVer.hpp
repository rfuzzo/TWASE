#pragma once

#include <cstdint>
#include <string>

struct SemVer
{
    struct PrereleaseInfo
    {
        uint32_t type;
        uint32_t number;
    };

    uint8_t major;
    uint16_t minor;
    uint32_t patch;
    PrereleaseInfo prerelease;
};

SemVer CreateSemVer(uint8_t aMajor, uint16_t aMinor, uint32_t aPatch, uint32_t prereleaseType,
                    uint32_t prereleaseNumber);

/**
 * @brief Compare two semantic versions.
 * @param aLhs The lhs version.
 * @param aRhs The rhs version.
 * @return < 0 if lhs is lower than rhs, 0 if they are equal, > 0 if lhs is greater than lhs.
 */
int32_t CompareSemVer(const SemVer& aLhs, const SemVer& aRhs);

/**
 * @brief Compare two pre-release semantic versions.
 * @param aLhs The lhs pre-release.
 * @param aRhs The rhs pre-release.
 * @return < 0 if lhs is lower than rhs, 0 if they are equal, > 0 if lhs is greater than lhs.
 */
int32_t CompareSemVerPrerelease(const SemVer::PrereleaseInfo& aLhs,
                                const SemVer::PrereleaseInfo& aRhs);

// clang-format off
#define TWASE_SEMVER_PRERELEASE_TYPE_NONE     0ul
#define TWASE_SEMVER_PRERELEASE_TYPE_ALPHA    1ul
#define TWASE_SEMVER_PRERELEASE_TYPE_BETA     2ul
#define TWASE_SEMVER_PRERELEASE_TYPE_RC       3ul
// clang-format on

#define TWASE_SEMVER_EX(major, minor, patch, prereleaseType, prereleaseNumber)                                    \
    CreateSemVer(major, minor, patch, prereleaseType, prereleaseNumber)

#define TWASE_SEMVER(major, minor, patch)                                                                         \
    TWASE_SEMVER_EX(major, minor, patch, TWASE_SEMVER_PRERELEASE_TYPE_NONE, 0)

#ifdef __cplusplus
namespace std
{
std::wstring to_wstring(const SemVer& aVersion);
} // namespace std
#endif

#ifdef __cplusplus
#pragma region PrereleaseInfo operators
inline bool operator<(const SemVer::PrereleaseInfo& aLhs, const SemVer::PrereleaseInfo& aRhs)
{
    return CompareSemVerPrerelease(aLhs, aRhs) < 0;
}

inline bool operator<=(const SemVer::PrereleaseInfo& aLhs, const SemVer::PrereleaseInfo& aRhs)
{
    return CompareSemVerPrerelease(aLhs, aRhs) <= 0;
}

inline bool operator>(const SemVer::PrereleaseInfo& aLhs, const SemVer::PrereleaseInfo& aRhs)
{
    return CompareSemVerPrerelease(aLhs, aRhs) > 0;
}

inline bool operator>=(const SemVer::PrereleaseInfo& aLhs, const SemVer::PrereleaseInfo& aRhs)
{
    return CompareSemVerPrerelease(aLhs, aRhs) >= 0;
}

inline bool operator==(const SemVer::PrereleaseInfo& aLhs, const SemVer::PrereleaseInfo& aRhs)
{
    return CompareSemVerPrerelease(aLhs, aRhs) == 0;
}

inline bool operator!=(const SemVer::PrereleaseInfo& aLhs, const SemVer::PrereleaseInfo& aRhs)
{
    return CompareSemVerPrerelease(aLhs, aRhs) != 0;
}
#pragma endregion

#pragma region VersionInfo operators
inline bool operator<(const SemVer& aLhs, const SemVer& aRhs)
{
    return CompareSemVer(aLhs, aRhs) < 0;
}

inline bool operator<=(const SemVer& aLhs, const SemVer& aRhs)
{
    return CompareSemVer(aLhs, aRhs) <= 0;
}

inline bool operator>(const SemVer& aLhs, const SemVer& aRhs)
{
    return CompareSemVer(aLhs, aRhs) > 0;
}

inline bool operator>=(const SemVer& aLhs, const SemVer& aRhs)
{
    return CompareSemVer(aLhs, aRhs) >= 0;
}

inline bool operator==(const SemVer& aLhs, const SemVer& aRhs)
{
    return CompareSemVer(aLhs, aRhs) == 0;
}

inline bool operator!=(const SemVer& aLhs, const SemVer& aRhs)
{
    return CompareSemVer(aLhs, aRhs) != 0;
}
#pragma endregion
#endif

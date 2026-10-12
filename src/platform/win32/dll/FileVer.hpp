#pragma once

#include <cstdint>
#include <string>


struct FileVer
{
    uint16_t major;
    uint16_t minor;
    uint16_t build;
    uint16_t revision;
};

FileVer CreateFileVer(uint16_t aMajor, uint16_t aMinor, uint16_t aBuild, uint16_t aRevision);

/**
 * @brief Compare two file versions.
 * @param aLhs The lhs version.
 * @param aRhs The rhs version.
 * @return < 0 if lhs is lower than rhs, 0 if they are equal, > 0 if lhs is greater than lhs.
 */
int32_t CompareFileVer(const FileVer& aLhs, const FileVer& aRhs);


#define TWASE_FILEVER(major, minor, build, revision) CreateFileVer(major, minor, build, revision)

#ifdef __cplusplus
namespace std
{
std::wstring to_wstring(const FileVer& aVersion);
} // namespace std
#endif

#ifdef __cplusplus
inline bool operator<(const FileVer& aLhs, const FileVer& aRhs)
{
    return CompareFileVer(aLhs, aRhs) < 0;
}

inline bool operator<=(const FileVer& aLhs, const FileVer& aRhs)
{
    return CompareFileVer(aLhs, aRhs) <= 0;
}

inline bool operator>(const FileVer& aLhs, const FileVer& aRhs)
{
    return CompareFileVer(aLhs, aRhs) > 0;
}

inline bool operator>=(const FileVer& aLhs, const FileVer& aRhs)
{
    return CompareFileVer(aLhs, aRhs) >= 0;
}

inline bool operator==(const FileVer& aLhs, const FileVer& aRhs)
{
    return CompareFileVer(aLhs, aRhs) == 0;
}

inline bool operator!=(const FileVer& aLhs, const FileVer& aRhs)
{
    return CompareFileVer(aLhs, aRhs) != 0;
}
#endif

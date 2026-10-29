#pragma once

#include <cstdint>
#include <string_view>

namespace sdk::Attila
{
enum class Build { Unknown, Steam, GOG };

// empire.retail.dll build IDs: CodeView PDB GUID + age, symbol server form (Image::SelectBuild). docs/game-versions.md
// Steam: file version 1.6.0.0, PE timestamp 0x69CE1786 (2026-04-02), SizeOfImage 0x2B9C000,
// SHA-256 8A40CBCFF108E1CBA14CC7EB6C7F4DFB054C7745B236D1BFC0FDE5C85E2A353D
inline constexpr std::string_view SteamBuildId = "D49769ED8EE04D6F8D1ADE5F582D19BB1";
// GOG 1.6.7 (GOG build 59847060534356892): file version 1.6.0.0, PE timestamp 0x6A4F4723 (2026-07-09),
// SizeOfImage 0x2B9C000, SHA-256 0C0BE767D42C1B7D249E0BF1CFE7AB97C03F4F7B341E077A38909CA5E0E0DF70
inline constexpr std::string_view GogBuildId = "38260D892D1D47DEBF34EA9A6420E4C71";

namespace detail
{
// Selected by Image before any patch, hook, or game API initialization.
inline constinit Build selectedBuild = Build::Unknown;
}

// RVAs in empire.retail.dll, one per supported build. Offsets within objects remain ordinary integers.
// DLL globals are initialized before the build is selected: keep a reference and convert when it's used.
struct BuildAddress
{
    uint32_t steam;
    uint32_t gog;

    operator uint32_t() const
    {
        switch (detail::selectedBuild)
        {
        case Build::Steam: return steam;
        case Build::GOG: return gog;
        case Build::Unknown: return 0;
        }
        return 0;
    }
};
} // namespace sdk::Attila

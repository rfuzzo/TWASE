#pragma once

#include <cstdint>
#include <string_view>

namespace sdk::Attila
{
enum class Build { Unknown, Steam, AppStore };

inline constexpr std::string_view SteamUUID = "392D6F66-9183-329E-8A98-8C90FC828326";
inline constexpr std::string_view AppStoreUUID = "196C74A0-71A2-3C28-80DF-BC3DB1C7FEB7";

namespace detail
{
// Selected by Image before any patch, hook, or game API initialization.
inline constinit Build selectedBuild = Build::Unknown;
}

// Offsets within objects remain ordinary integers. Only executable addresses vary by build.
// Keep these objects constant-initialized: dylib constructors run before dynamic initializers.
struct BuildAddress
{
    uint64_t steam;
    uint64_t appStore;

    operator uint64_t() const
    {
        switch (detail::selectedBuild)
        {
        case Build::Steam: return steam;
        case Build::AppStore: return appStore;
        case Build::Unknown: return 0;
        }
        return 0;
    }
};
} // namespace sdk::Attila

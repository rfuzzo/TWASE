#pragma once

#include <cstdint>

namespace sdk::Attila
{
    // How the address is taken from a pattern match
    enum class SignatureType : uint8_t
    {
        Direct, // match + offset
        Rel32,  // the rel32 at match + offset (e.g. of a call) resolves to its target
        Abs32,  // the absolute address stored at match + offset (e.g. a global in a mov/cmp)
    };

    // IDA style byte pattern ("??" is a wildcard), unique in empire.retail.dll.
    // Relocated addresses and call/jump displacements are wildcarded so the patterns survive small game updates.
    struct Signature
    {
        const char* pattern;
        SignatureType type = SignatureType::Direct;
        int32_t offset = 0;
    };

} // namespace sdk::Attila

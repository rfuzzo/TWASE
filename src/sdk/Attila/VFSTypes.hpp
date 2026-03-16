#pragma once

namespace sdk::Attila
{
    struct TempString {
        unsigned int length;
        unsigned int capacity;
        char* data;
    };

    struct CName {
        const char* pooled;
    };

    struct VFSSearchResults {
        int    count;
        int    capacity;
        void** entries;
    };

} // namespace sdk::Attila

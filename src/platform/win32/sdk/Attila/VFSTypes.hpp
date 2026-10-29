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

    // filled by VFS::SearchFiles, free entries with tw_free. Capacity comes first (as on macOS): read as the count,
    // it went 1, 2, 4 and a third mod made the loader read an unfilled entry and crash.
    struct VFSSearchResults {
        unsigned int capacity;
        unsigned int count;
        void**       entries;
    };
    static_assert(sizeof(VFSSearchResults) == 12);

} // namespace sdk::Attila

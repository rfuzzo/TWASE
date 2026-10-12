#pragma once

#include <cstddef>
#include <cstdint>

namespace sdk::Attila
{
// game string (CA::String), allocated with the game allocator
struct TempString
{
    uint32_t length;   // +0x00
    uint32_t capacity; // +0x04
    char* data;        // +0x08
};
static_assert(sizeof(TempString) == 0x10);

// pooled name, CName_ctor interns the string
struct CName
{
    const char* pooled;
};

// pooled path record a search result points to
struct VFSEntry
{
    uint64_t field_0;  // +0x00
    const char* path;  // +0x08 UTF-8, backslashes, e.g. "campaigns\main_attila\mods\x\scripting.lua"
};

// filled by VFS::SearchFiles, free entries with tw_free
struct VFSSearchResults
{
    uint32_t capacity;    // +0x00
    uint32_t count;       // +0x04
    VFSEntry** entries;   // +0x08
};
static_assert(sizeof(VFSSearchResults) == 0x10);

} // namespace sdk::Attila

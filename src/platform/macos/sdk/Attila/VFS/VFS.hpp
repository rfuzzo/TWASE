#pragma once

#include "../ScriptInterface.hpp"
#include "../VFSTypes.hpp"

#include <atomic>
#include <string>
#include <vector>

using namespace sdk::Attila;

class VFS
{
public:
    // aSlide: ASLR slide of the game executable
    static void Init(uintptr_t aSlide);
    static bool IsReady()
    {
        return s_ready.load(std::memory_order_acquire);
    }

private:
    static std::atomic<bool> s_ready;

public:
    // a virtual of the VFS instance (VFS_SearchFiles_VtableOffset), dir and pattern are passed by reference
    using VFS_SearchFiles_t = void (*)(void* vfs, const CName* baseDir, const CName* pattern, VFSSearchResults* out,
                                       int flags, int mode);
    using VFS_GetInstance_t = void* (*)();
    using CName_ctor_t = CName* (*)(CName* self, const char* str);
    using tw_free_t = void (*)(void*);

public:
    static void* VFSGetInstance();
    static void VFSSearchFiles(void* vfs, const CName* baseDir, const CName* pattern, VFSSearchResults* out,
                               int flags = 0, int mode = 3);

    static CName* CName_ctor(CName* self, const char* str);

    static void tw_free(void* ptr);

private:
    static VFS_GetInstance_t s_VFSGetInstance;

    static CName_ctor_t s_CName_ctor;
    static tw_free_t s_tw_free;
};

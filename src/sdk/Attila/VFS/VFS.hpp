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
    static void Init(uint32_t empireDllBase);
    static bool IsReady()
    {
        return s_ready.load(std::memory_order_acquire);
    }

private:
    static std::atomic<bool>   s_ready;

public:
    typedef void(__thiscall* VFS_SearchFiles_t)(
        void* vfs,
        const char* baseDir,
        const char* pattern,
        VFSSearchResults* out,
        int flags,
        int mode
        );
    typedef void* (__cdecl* VFS_GetInstance_t)();
    typedef CName* (__thiscall* CName_ctor_t)(CName* self, const char* str);

public: 
    static void* VFSGetInstance();
    static void VFSSearchFiles(
        void* vfs,
        const char* baseDir,
        const char* pattern,
        VFSSearchResults* out,
        int flags = 0,
        int mode = 3
	);
	static CName* CName_ctor(CName* self, const char* str);
private:
    static VFS_GetInstance_t s_VFSGetInstance;
    static VFS_SearchFiles_t s_VFSSearchFiles;
    static CName_ctor_t s_CName_ctor;
};
#include "VFS.hpp"
#include "../Addresses.hpp"

#include <spdlog/spdlog.h>

using namespace sdk::Attila;

// static member definitions
std::atomic<bool> VFS::s_ready = false;

VFS::VFS_GetInstance_t  VFS::s_VFSGetInstance = nullptr;
VFS::VFS_SearchFiles_t  VFS::s_VFSSearchFiles = nullptr;

VFS::CName_ctor_t       VFS::s_CName_ctor = nullptr;
VFS::tw_free_t          VFS::s_tw_free = nullptr;

void VFS::Init(uint32_t base)
{
    s_VFSGetInstance    = reinterpret_cast<VFS_GetInstance_t>(base + Addresses::VFS_GetInstance);
	s_VFSSearchFiles    = reinterpret_cast<VFS_SearchFiles_t>(base + Addresses::VFS_SearchFiles);

    s_CName_ctor        = reinterpret_cast<CName_ctor_t>(base + Addresses::CName_ctor);
	s_tw_free           = reinterpret_cast<tw_free_t>(base + Addresses::tw_free);

    spdlog::info("VFS resolved all function pointers");

    s_ready.store(true, std::memory_order_release);
}

void* VFS::VFSGetInstance()
{
    if (!s_VFSGetInstance) return nullptr;
    return s_VFSGetInstance();
}

void VFS::VFSSearchFiles(
    void* vfs,
    const char* baseDir,
    const char* pattern,
    VFSSearchResults* out,
    int flags,
    int mode)
{
    if (!s_VFSSearchFiles || !vfs) return;
    s_VFSSearchFiles(vfs, baseDir, pattern, out, flags, mode);
}

CName* VFS::CName_ctor(CName* self, const char* str)
{
    if (!s_CName_ctor) return nullptr;
    return s_CName_ctor(self, str);
}

void VFS::tw_free(void* ptr)
{
    if (!s_tw_free || !ptr) return;
    s_tw_free(ptr);
}
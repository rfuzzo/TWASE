#include "VFS.hpp"
#include "../Addresses.hpp"

#include <spdlog/spdlog.h>

using namespace sdk::Attila;

// static member definitions
std::atomic<bool> VFS::s_ready = false;

VFS::VFS_GetInstance_t VFS::s_VFSGetInstance = nullptr;

VFS::CName_ctor_t VFS::s_CName_ctor = nullptr;
VFS::tw_free_t VFS::s_tw_free = nullptr;

void VFS::Init(uintptr_t aSlide)
{
    s_VFSGetInstance = reinterpret_cast<VFS_GetInstance_t>(Addresses::VFS_GetInstance + aSlide);

    s_CName_ctor = reinterpret_cast<CName_ctor_t>(Addresses::CName_ctor + aSlide);
    s_tw_free = reinterpret_cast<tw_free_t>(Addresses::tw_free + aSlide);

    spdlog::info("VFS resolved all function pointers");

    s_ready.store(true, std::memory_order_release);
}

void* VFS::VFSGetInstance()
{
    if (!s_VFSGetInstance) return nullptr;
    return s_VFSGetInstance();
}

void VFS::VFSSearchFiles(void* vfs, const CName* baseDir, const CName* pattern, VFSSearchResults* out, int flags,
                         int mode)
{
    if (!vfs) return;

    auto vtable = *reinterpret_cast<void***>(vfs);
    auto searchFiles = reinterpret_cast<VFS_SearchFiles_t>(vtable[Addresses::VFS_SearchFiles_VtableOffset / sizeof(void*)]);
    searchFiles(vfs, baseDir, pattern, out, flags, mode);
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

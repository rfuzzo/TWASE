// Phase 0 spike: verifies dylib injection and whether the hardened runtime lets us modify __TEXT.
//
// TWASE_SPIKE_PATCH selects the test:
//   0 (default) only log
//   1 rewrite the page holding the entry point with identical bytes via mach_vm_protect(VM_PROT_COPY)
//   2 replace that page with an anonymous RX copy via mach_vm_remap (relies on allow-unsigned-executable-memory)
//
// main() runs from the rewritten page right after the constructors, so a code signing kill shows up immediately.

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include <dlfcn.h>
#include <libkern/OSCacheControl.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int csops(pid_t pid, unsigned int ops, void* useraddr, size_t usersize);

namespace
{
FILE* g_log = nullptr;

void Log(const char* fmt, ...)
{
    if (!g_log)
        return;

    char ts[32];
    time_t now = time(nullptr);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&now));
    fprintf(g_log, "[%s] [%d] ", ts, getpid());

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);

    fputc('\n', g_log);
    fflush(g_log); // the process may get killed right after
}

std::string ParentDir(const std::string& path, int levels)
{
    std::string result = path;
    for (int i = 0; i < levels; i++)
        result = result.substr(0, result.find_last_of('/'));
    return result;
}

void OpenLog(const std::string& exePath)
{
    // <root>/Total War ATTILA.app/Contents/MacOS/Total War ATTILA -> <root>/TWASE/logs/spike.log
    auto twaseDir = ParentDir(exePath, 4) + "/TWASE";
    auto logsDir = twaseDir + "/logs";
    mkdir(twaseDir.c_str(), 0755);
    mkdir(logsDir.c_str(), 0755);
    g_log = fopen((logsDir + "/spike.log").c_str(), "a");
}

void LogCodeSigningFlags()
{
    uint32_t flags = 0;
    if (csops(getpid(), 0 /* CS_OPS_STATUS */, &flags, sizeof(flags)) != 0)
    {
        Log("csops failed");
        return;
    }

    struct Flag { uint32_t bit; const char* name; };
    constexpr Flag known[] = {
        {0x1, "VALID"}, {0x2, "ADHOC"}, {0x4, "GET_TASK_ALLOW"}, {0x10, "FORCED_LV"},
        {0x20, "INVALID_ALLOWED"}, {0x100, "HARD"}, {0x200, "KILL"}, {0x800, "RESTRICT"},
        {0x1000, "ENFORCEMENT"}, {0x2000, "REQUIRE_LV"}, {0x10000, "RUNTIME"},
        {0x1000000, "KILLED"}, {0x10000000, "DEBUGGED"},
    };

    std::string names;
    for (const auto& flag : known)
    {
        if (flags & flag.bit)
            names += std::string(flag.name) + " ";
    }
    Log("code signing flags: %#x ( %s)", flags, names.c_str());
}

// with DYLD_INSERT_LIBRARIES the inserted dylibs can come before the executable, so don't assume index 0
uint32_t MainImageIndex()
{
    for (uint32_t i = 0; i < _dyld_image_count(); i++)
    {
        if (_dyld_get_image_header(i)->filetype == MH_EXECUTE)
            return i;
    }
    return 0;
}

const mach_header_64* MainImage()
{
    return reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(MainImageIndex()));
}

template<typename F>
void ForEachLoadCommand(const mach_header_64* header, F&& fn)
{
    auto cmd = reinterpret_cast<const load_command*>(header + 1);
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        fn(cmd);
        cmd = reinterpret_cast<const load_command*>(reinterpret_cast<const uint8_t*>(cmd) + cmd->cmdsize);
    }
}

void LogImageInfo()
{
    auto header = MainImage();
    Log("main image: %s", _dyld_get_image_name(MainImageIndex()));
    Log("header %p, slide %#lx", header, _dyld_get_image_vmaddr_slide(MainImageIndex()));

    ForEachLoadCommand(header, [](const load_command* cmd) {
        if (cmd->cmd == LC_UUID)
        {
            auto uuid = reinterpret_cast<const uuid_command*>(cmd)->uuid;
            Log("LC_UUID %02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", uuid[0], uuid[1],
                uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11], uuid[12],
                uuid[13], uuid[14], uuid[15]);
        }
    });
}

uint8_t* EntryPoint()
{
    auto header = MainImage();
    uint8_t* entry = nullptr;
    ForEachLoadCommand(header, [&](const load_command* cmd) {
        if (cmd->cmd == LC_MAIN)
        {
            auto offset = reinterpret_cast<const entry_point_command*>(cmd)->entryoff;
            entry = reinterpret_cast<uint8_t*>(const_cast<mach_header_64*>(header)) + offset;
        }
    });
    return entry;
}

// test 1: make the code page writable (copy on write), write identical bytes, make it executable again
bool RewriteWithVmProtect(uint8_t* address)
{
    auto page = reinterpret_cast<mach_vm_address_t>(address) & ~static_cast<mach_vm_address_t>(vm_page_size - 1);

    auto kr = mach_vm_protect(mach_task_self(), page, vm_page_size, false,
                              VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    if (kr != KERN_SUCCESS)
    {
        Log("mach_vm_protect(RW|COPY) failed: %s", mach_error_string(kr));
        return false;
    }

    uint32_t original;
    memcpy(&original, address, sizeof(original));
    memcpy(address, &original, sizeof(original));

    kr = mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ | VM_PROT_EXECUTE);
    if (kr != KERN_SUCCESS)
    {
        Log("mach_vm_protect(RX) failed: %s", mach_error_string(kr));
        return false;
    }

    sys_icache_invalidate(reinterpret_cast<void*>(page), vm_page_size);
    Log("rewrote page %#llx via mach_vm_protect (instruction %08x)", page, original);
    return true;
}

// test 2: copy the page into anonymous memory, make that RX and map it over the original page
bool RewriteWithRemap(uint8_t* address)
{
    auto page = reinterpret_cast<mach_vm_address_t>(address) & ~static_cast<mach_vm_address_t>(vm_page_size - 1);

    void* copy = mmap(nullptr, vm_page_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (copy == MAP_FAILED)
    {
        Log("mmap failed: %s", strerror(errno));
        return false;
    }

    memcpy(copy, reinterpret_cast<void*>(page), vm_page_size);
    if (mprotect(copy, vm_page_size, PROT_READ | PROT_EXEC) != 0)
    {
        Log("mprotect(copy, RX) failed: %s", strerror(errno));
        return false;
    }

    mach_vm_address_t target = page;
    vm_prot_t cur = 0, max = 0;
    auto kr = mach_vm_remap(mach_task_self(), &target, vm_page_size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                            mach_task_self(), reinterpret_cast<mach_vm_address_t>(copy), false, &cur, &max,
                            VM_INHERIT_COPY);
    if (kr != KERN_SUCCESS)
    {
        Log("mach_vm_remap failed: %s", mach_error_string(kr));
        return false;
    }

    sys_icache_invalidate(reinterpret_cast<void*>(page), vm_page_size);
    Log("remapped page %#llx from anonymous memory (cur %d, max %d)", page, cur, max);
    return true;
}

__attribute__((constructor)) void SpikeInit()
{
    char exePath[4096];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) != 0)
        return;

    OpenLog(exePath);
    Log("---- spike loaded into %s", exePath);

    auto inserted = getenv("DYLD_INSERT_LIBRARIES");
    Log("DYLD_INSERT_LIBRARIES=%s", inserted ? inserted : "(unset)");
    Log("parent pid %d", getppid());

    LogCodeSigningFlags();
    LogImageInfo();

    auto mode = getenv("TWASE_SPIKE_PATCH");
    auto test = mode ? atoi(mode) : 0;
    auto entry = EntryPoint();
    Log("entry point %p, patch test %d", entry, test);

    if (entry && test == 1)
        RewriteWithVmProtect(entry);
    else if (entry && test == 2)
        RewriteWithRemap(entry);

    Log("constructor done, handing over to main()");
}

__attribute__((destructor)) void SpikeExit()
{
    Log("process exiting normally");
}
} // namespace

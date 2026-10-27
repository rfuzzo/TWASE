#include "stdafx.hpp"

#include "TamperResponse.hpp"
#include "Image.hpp"

#include <execinfo.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

namespace
{
// Function-object call slots (vtable entries in __DATA_CONST) of Feral's termination responses. Each is replaced with a
// function that only logs. Data, not code, so the game's code hashes don't see it.
struct Response
{
    const char* name;
    uint64_t slot;
    uint64_t target;
};

constexpr Response Responses[] = {
    // AntiTamper_Response_ExhaustMemory_DelayedThread: sleeps, then grows malloc until it fails (session L)
    {"memory exhaustion", 0x104b1e450, 0x100f67afc},
    // sub_100fbcb58: recurses until the thread's stack guard (session P), called from the delayed job sub_100fbc9c0
    {"stack exhaustion", 0x104b21490, 0x100fbcb58},
};

template<size_t Index>
int64_t Disarmed(void*)
{
    spdlog::warn("[TamperResponse] Feral's {} response fired, disarmed", Responses[Index].name);
    return 0;
}

template<size_t... Indexes>
constexpr auto MakeDisarmed(std::index_sequence<Indexes...>)
{
    return std::array<int64_t (*)(void*), sizeof...(Indexes)>{&Disarmed<Indexes>...};
}

constexpr auto DisarmedFunctions = MakeDisarmed(std::make_index_sequence<std::size(Responses)>());

// game-relative address of a code pointer, for logging
uint64_t Unslid(uintptr_t aAddress)
{
    const auto slide = Image::Get()->GetSlide();
    return aAddress >= 0x100000000 + slide && aAddress < 0x1056d8000 + slide ? aAddress - slide : aAddress;
}

// game-relative return addresses of the caller, for logging
std::string Backtrace()
{
    void* frames[32];
    const int count = backtrace(frames, std::size(frames));
    std::string text;
    for (int i = 2; i < count; i++)
    {
        text += fmt::format(" {:#x}", Unslid(reinterpret_cast<uintptr_t>(frames[i])));
    }
    return text;
}

// the ways the game can end itself, each logged with its caller and then passed on unchanged
void LoggedExit(int aStatus)
{
    spdlog::warn("[TamperResponse] exit({}) called from:{}", aStatus, Backtrace());
    spdlog::default_logger()->flush();
    exit(aStatus);
}

void LoggedUnderscoreExit(int aStatus)
{
    spdlog::warn("[TamperResponse] _exit({}) called from:{}", aStatus, Backtrace());
    spdlog::default_logger()->flush();
    _exit(aStatus);
}

void LoggedCapitalExit(int aStatus)
{
    spdlog::warn("[TamperResponse] _Exit({}) called from:{}", aStatus, Backtrace());
    spdlog::default_logger()->flush();
    _Exit(aStatus);
}

void LoggedAbort()
{
    spdlog::warn("[TamperResponse] abort() called from:{}", Backtrace());
    spdlog::default_logger()->flush();
    abort();
}

int LoggedKill(pid_t aPid, int aSignal)
{
    if (aPid == getpid() || aPid == 0)
    {
        spdlog::warn("[TamperResponse] kill(self, {}) called from:{}", aSignal, Backtrace());
        spdlog::default_logger()->flush();
    }
    return kill(aPid, aSignal);
}

int LoggedRaise(int aSignal)
{
    spdlog::warn("[TamperResponse] raise({}) called from:{}", aSignal, Backtrace());
    spdlog::default_logger()->flush();
    return raise(aSignal);
}

// The game starts its detached jobs (Thread_StartDetached 0x100092f14) with the trampoline 0x10009301c and a heap copy
// of a std::function: the function object pointer is at +0x18, its vtable's call slot is entry 6. Logs the call target
// of every such job, to find the termination responses as they start.
int LoggedPthreadCreate(pthread_t* aThread, const pthread_attr_t* aAttr, void* (*aStart)(void*), void* aArg)
{
    if (Image::Get()->IsSupported() && reinterpret_cast<uintptr_t>(aStart) == Image::Get()->Resolve(0x10009301c) &&
        aArg)
    {
        const auto* function = *reinterpret_cast<void* const*>(static_cast<uint8_t*>(aArg) + 0x18);
        if (function)
        {
            const auto* vtable = *reinterpret_cast<const uintptr_t* const*>(function);
            void* frames[4];
            const int count = backtrace(frames, std::size(frames));
            spdlog::debug("[TamperResponse] detached job: call {:#x} (vtable slot {:#x}), started from {:#x}",
                          Unslid(vtable[6]), Unslid(reinterpret_cast<uintptr_t>(&vtable[6])),
                          count > 2 ? Unslid(reinterpret_cast<uintptr_t>(frames[2])) : 0);
        }
    }
    return pthread_create(aThread, aAttr, aStart, aArg);
}
} // namespace

// dyld interposing: every image's calls to exit() and pthread_create() go to the logging versions; their own calls to
// the originals aren't interposed
struct Interpose
{
    const void* replacement;
    const void* replacee;
};

__attribute__((used, section("__DATA,__interpose"))) static const Interpose interposes[] = {
    {reinterpret_cast<const void*>(&LoggedExit), reinterpret_cast<const void*>(&exit)},
    {reinterpret_cast<const void*>(&LoggedUnderscoreExit), reinterpret_cast<const void*>(&_exit)},
    {reinterpret_cast<const void*>(&LoggedCapitalExit), reinterpret_cast<const void*>(&_Exit)},
    {reinterpret_cast<const void*>(&LoggedAbort), reinterpret_cast<const void*>(&abort)},
    {reinterpret_cast<const void*>(&LoggedKill), reinterpret_cast<const void*>(&kill)},
    {reinterpret_cast<const void*>(&LoggedRaise), reinterpret_cast<const void*>(&raise)},
    {reinterpret_cast<const void*>(&LoggedPthreadCreate), reinterpret_cast<const void*>(&pthread_create)},
};

void TamperResponse::DisarmResponses()
{
    if (!Image::Get()->IsSupported())
    {
        return;
    }

    for (size_t i = 0; i < std::size(Responses); i++)
    {
        const auto& response = Responses[i];
        auto* slot = Image::Get()->Resolve<uint64_t>(response.slot);
        if (*slot != Image::Get()->Resolve(response.target))
        {
            spdlog::warn("[TamperResponse] {} slot {:#x} holds {:#x}, not the expected target, left alone",
                         response.name, response.slot, *slot);
            continue;
        }

        // __DATA_CONST is read-only after dyld's fixups
        const auto page =
            reinterpret_cast<mach_vm_address_t>(slot) & ~static_cast<mach_vm_address_t>(vm_page_size - 1);
        if (mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ | VM_PROT_WRITE) != KERN_SUCCESS)
        {
            spdlog::error("[TamperResponse] could not make the {} slot writable", response.name);
            continue;
        }
        *slot = reinterpret_cast<uint64_t>(DisarmedFunctions[i]);
        mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ);

        spdlog::info("[TamperResponse] {} response disarmed (slot {:#x})", response.name, response.slot);
    }
}

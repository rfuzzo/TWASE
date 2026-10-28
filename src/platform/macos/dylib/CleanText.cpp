#include "stdafx.hpp"

#include "CleanText.hpp"
#include "Image.hpp"
#include "Hooking/Memory.hpp"

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/loader.h>

#include <cstdlib>

bool Integrity::InstallCleanTextAnchor()
{
    static bool installed = false;
    if (installed)
    {
        return true;
    }
    const auto* image = Image::Get();
    if (!image->IsSupported())
    {
        return false;
    }

    constexpr uint64_t ImageBase = 0x100000000;
    const uint64_t Anchor = image->IsAppStore() ? 0x10025ca1c : 0x10025cc64;
    const uint64_t AnchorSlot = image->IsAppStore() ? 0x104a7f5a0 : 0x104a7f590;
    const uint64_t AnchorCall = image->IsAppStore() ? 0x100f6459c : 0x100f66b54;
    const uint64_t WriteProbe = image->IsAppStore() ? 0x10025cac8 : 0x10025cd10;
    constexpr uint32_t ExpectedProbe = 0xa9be4ff4; // stp x20, x19, [sp, #-0x20]!
    constexpr uint32_t Return = 0xd65f03c0;
    // Redirect only the startup BL, not the weak-bind stub still used by the hash guards.
    const uint32_t ExpectedCall = image->IsAppStore() ? 0x94c78524 : 0x94c7816f;
    const uint32_t DirectCall =
        0x94000000 | (static_cast<uint32_t>((int64_t(Anchor) - int64_t(AnchorCall)) / 4) & 0x03ffffff);
    auto* slot = image->Resolve<uintptr_t>(AnchorSlot);
    const auto call = image->Resolve(AnchorCall);
    const auto probe = image->Resolve(WriteProbe);
    if (*slot != image->Resolve(Anchor) || *reinterpret_cast<const uint32_t*>(call) != ExpectedCall ||
        *reinterpret_cast<const uint32_t*>(probe) != ExpectedProbe)
    {
        spdlog::error("[Integrity] Unexpected anchor slot, call or write probe; clean text anchor not installed");
        return false;
    }

    const auto* header = image->Resolve<const mach_header_64>(ImageBase);
    const segment_command_64* text = nullptr;
    auto* command = reinterpret_cast<const load_command*>(header + 1);
    for (uint32_t i = 0; i < header->ncmds; ++i)
    {
        if (command->cmd == LC_SEGMENT_64)
        {
            const auto* segment = reinterpret_cast<const segment_command_64*>(command);
            if (std::strcmp(segment->segname, "__TEXT") == 0)
            {
                text = segment;
                break;
            }
        }
        command = reinterpret_cast<const load_command*>(reinterpret_cast<const uint8_t*>(command) + command->cmdsize);
    }
    if (!text || Anchor < text->vmaddr || Anchor >= text->vmaddr + text->vmsize)
    {
        spdlog::error("[Integrity] Missing __TEXT segment containing the anchor");
        return false;
    }

    mach_vm_address_t copy = 0;
    auto kr = mach_vm_allocate(mach_task_self(), &copy, text->vmsize, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS)
    {
        spdlog::error("[Integrity] Cannot allocate clean __TEXT: {}", mach_error_string(kr));
        return false;
    }
    std::memcpy(reinterpret_cast<void*>(copy), image->Resolve<const void>(text->vmaddr), text->vmsize);
    // The snapshot is never executed; cap its maximum protection so it stays immutable for the lifetime of the process.
    kr = mach_vm_protect(mach_task_self(), copy, text->vmsize, true, VM_PROT_READ);
    if (kr != KERN_SUCCESS)
    {
        spdlog::error("[Integrity] Cannot seal clean __TEXT: {}", mach_error_string(kr));
        mach_vm_deallocate(mach_task_self(), copy, text->vmsize);
        return false;
    }

    const auto page = reinterpret_cast<mach_vm_address_t>(slot) & ~static_cast<mach_vm_address_t>(vm_page_size - 1);
    kr = mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS)
    {
        spdlog::error("[Integrity] Cannot make anchor slot writable: {}", mach_error_string(kr));
        mach_vm_deallocate(mach_task_self(), copy, text->vmsize);
        return false;
    }
    bool callAttempted = false;
    bool probeAttempted = false;
    bool published = false;
    const auto stopOnRollbackFailure = []
    {
        // Continuing with exposed modifications can trigger delayed game failures. Do not disguise this as a
        // successful rollback or free a snapshot that the game might still reference.
        spdlog::critical("[Integrity] Clean text anchor rollback failed; stopping before the game runs");
        spdlog::default_logger()->flush();
        std::_Exit(EXIT_FAILURE);
    };
    const auto rollback = [&]() -> bool
    {
        if (published)
        {
            const auto writable =
                mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ | VM_PROT_WRITE);
            if (writable != KERN_SUCCESS)
            {
                stopOnRollbackFailure();
            }
            *slot = image->Resolve(Anchor);
        }

        // WriteCode can fail after copying bytes (for example, while restoring RX protection). Restore every
        // attempted write, not just writes that reported success, and try both restorations even if one fails.
        bool restored = true;
        if (probeAttempted)
        {
            restored &= Memory::WriteCode(probe, &ExpectedProbe, sizeof(ExpectedProbe));
        }
        if (callAttempted)
        {
            restored &= Memory::WriteCode(call, &ExpectedCall, sizeof(ExpectedCall));
        }
        restored &= mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ) == KERN_SUCCESS;
        if (!restored)
        {
            stopOnRollbackFailure();
        }
        mach_vm_deallocate(mach_task_self(), copy, text->vmsize);
        spdlog::error("[Integrity] Clean text anchor installation failed; original code and anchor restored");
        return false;
    };

    // The write probe either corrupts the anchor byte or increments the tamper counter on permission denial. Neither
    // is meaningful for the snapshot. Disable it in live code only; the clean copy retains its original instructions.
    callAttempted = true;
    if (!Memory::WriteCode(call, &DirectCall, sizeof(DirectCall)))
    {
        return rollback();
    }
    probeAttempted = true;
    if (!Memory::WriteCode(probe, &Return, sizeof(Return)))
    {
        return rollback();
    }

    // Publish only after the original function is reachable without using this slot. Keep the snapshot for process life:
    // game threads and static destructors may still hash it after TWASE's own destructor runs.
    *slot = copy + Anchor - text->vmaddr;
    published = true;
    kr = mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ);
    if (kr != KERN_SUCCESS)
    {
        spdlog::error("[Integrity] Cannot restore anchor slot protection: {}", mach_error_string(kr));
        return rollback();
    }
    installed = true;
    spdlog::info("[Integrity] Clean __TEXT anchor installed: {:#x} bytes at {:#x}, anchor {:#x}; startup call uses live code",
                 text->vmsize, copy, *slot);
    return true;
}

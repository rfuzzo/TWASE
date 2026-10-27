#include "stdafx.hpp"

#include "Integrity.hpp"
#include "Image.hpp"
#include "Hooking/Memory.hpp"

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/loader.h>

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
    constexpr uint64_t Anchor = 0x10025cc64;
    constexpr uint64_t AnchorSlot = 0x104a7f590;
    constexpr uint64_t AnchorCall = 0x100f66b54;
    constexpr uint64_t WriteProbe = 0x10025cd10;
    constexpr uint32_t ExpectedProbe = 0xa9be4ff4; // stp x20, x19, [sp, #-0x20]!
    constexpr uint32_t Return = 0xd65f03c0;
    // BL to the weak-bind stub at 0x104147110. Redirect only this call, not the stub used by the hash guards.
    constexpr uint32_t ExpectedCall = 0x94c7816f;
    constexpr uint32_t DirectCall =
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
    // The write probe either corrupts the anchor byte or increments the tamper counter on permission denial. Neither
    // is meaningful for the snapshot. Disable it in live code only; the clean copy retains its original instructions.
    if (!Memory::WriteCode(call, &DirectCall, sizeof(DirectCall)) ||
        !Memory::WriteCode(probe, &Return, sizeof(Return)))
    {
        mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ);
        mach_vm_deallocate(mach_task_self(), copy, text->vmsize);
        return false;
    }

    // Publish only after the original function is reachable without using this slot. Keep the snapshot for process life:
    // game threads and static destructors may still hash it after TWASE's own destructor runs.
    *slot = copy + Anchor - text->vmaddr;
    kr = mach_vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ);
    if (kr != KERN_SUCCESS)
    {
        spdlog::error("[Integrity] Cannot restore anchor slot protection: {}", mach_error_string(kr));
        return false;
    }
    installed = true;
    spdlog::info("[Integrity] Clean __TEXT anchor installed: {:#x} bytes at {:#x}, anchor {:#x}; startup call uses live code",
                 text->vmsize, copy, *slot);
    return true;
}

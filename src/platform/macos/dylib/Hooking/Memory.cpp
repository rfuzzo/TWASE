#include "stdafx.hpp"

#include "Memory.hpp"
#include "../Image.hpp"

#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>

namespace
{
std::string ToHex(std::span<const uint8_t> aBytes)
{
    std::string result;
    for (auto b : aBytes)
    {
        result += fmt::format("{:02X} ", b);
    }

    if (!result.empty())
    {
        result.pop_back();
    }

    return result;
}

// Needs com.apple.security.cs.allow-unsigned-executable-memory, which the game has
bool WritePageByRemap(mach_vm_address_t aPage, size_t aOffset, const void* aData, size_t aSize)
{
    void* copy = mmap(nullptr, vm_page_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (copy == MAP_FAILED)
    {
        spdlog::warn("Memory: mmap failed: {}", strerror(errno));
        return false;
    }

    std::memcpy(copy, reinterpret_cast<const void*>(aPage), vm_page_size);
    std::memcpy(static_cast<uint8_t*>(copy) + aOffset, aData, aSize);

    if (mprotect(copy, vm_page_size, PROT_READ | PROT_EXEC) != 0)
    {
        spdlog::warn("Memory: mprotect(RX) failed: {}", strerror(errno));
        munmap(copy, vm_page_size);
        return false;
    }

    auto target = aPage;
    vm_prot_t cur = VM_PROT_NONE;
    vm_prot_t max = VM_PROT_NONE;
    auto kr = mach_vm_remap(mach_task_self(), &target, vm_page_size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                            mach_task_self(), reinterpret_cast<mach_vm_address_t>(copy), false, &cur, &max,
                            VM_INHERIT_COPY);

    // the remapped page keeps its own reference to the memory
    munmap(copy, vm_page_size);

    if (kr != KERN_SUCCESS)
    {
        spdlog::warn("Memory: mach_vm_remap failed: {}", mach_error_string(kr));
        return false;
    }

    return true;
}

bool WritePageByProtect(mach_vm_address_t aPage, size_t aOffset, const void* aData, size_t aSize)
{
    auto kr = mach_vm_protect(mach_task_self(), aPage, vm_page_size, false,
                              VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    if (kr != KERN_SUCCESS)
    {
        spdlog::warn("Memory: mach_vm_protect(RW) failed: {}", mach_error_string(kr));
        return false;
    }

    std::memcpy(reinterpret_cast<uint8_t*>(aPage) + aOffset, aData, aSize);

    kr = mach_vm_protect(mach_task_self(), aPage, vm_page_size, false, VM_PROT_READ | VM_PROT_EXECUTE);
    if (kr != KERN_SUCCESS)
    {
        spdlog::error("Memory: mach_vm_protect(RX) failed: {}", mach_error_string(kr));
        return false;
    }

    return true;
}
} // namespace

bool Memory::WriteCode(uintptr_t aAddress, const void* aData, size_t aSize)
{
    auto data = static_cast<const uint8_t*>(aData);

    while (aSize > 0)
    {
        auto page = static_cast<mach_vm_address_t>(aAddress & ~(vm_page_size - 1));
        auto offset = static_cast<size_t>(aAddress - page);
        auto count = std::min(aSize, static_cast<size_t>(vm_page_size) - offset);

        if (!WritePageByRemap(page, offset, data, count) && !WritePageByProtect(page, offset, data, count))
        {
            return false;
        }

        sys_icache_invalidate(reinterpret_cast<void*>(aAddress), count);

        aAddress += count;
        data += count;
        aSize -= count;
    }

    return true;
}

bool Memory::PatchBytes(std::string_view aName, uint64_t aAddress, std::span<const uint8_t> aExpected,
                        std::span<const uint8_t> aPatch)
{
    if (aExpected.size() != aPatch.size())
    {
        spdlog::error("{} has mismatching expected and patch sizes", aName);
        return false;
    }

    auto address = Image::Get()->Resolve(aAddress);
    std::span<const uint8_t> current(reinterpret_cast<const uint8_t*>(address), aPatch.size());

    if (std::ranges::equal(current, aPatch))
    {
        spdlog::info("{} is already applied at {:#x}", aName, aAddress);
        return true;
    }

    if (!std::ranges::equal(current, aExpected))
    {
        spdlog::warn("{} skipped: unexpected bytes at {:#x} ({}), the game version is probably not supported", aName,
                     aAddress, ToHex(current));
        return false;
    }

    if (!WriteCode(address, aPatch.data(), aPatch.size()))
    {
        spdlog::error("{} could not be written at {:#x}", aName, aAddress);
        return false;
    }

    spdlog::info("Applied {} at {:#x}", aName, aAddress);
    return true;
}

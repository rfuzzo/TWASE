#include "stdafx.hpp"

#include "Platform.hpp"
#include "Utils.hpp"
#include "Image.hpp"

#include <mach/mach.h>
#include <mach/mach_vm.h>

void Platform::ShowMessageBox(const std::string_view aText, bool aIsError)
{
    Utils::ShowMessageBox(aText, aIsError);
}

void Platform::Exit()
{
    Utils::Exit();
}

uintptr_t Platform::ResolveAddress(uint64_t aAddress)
{
    return static_cast<uintptr_t>(aAddress) + Image::Get()->GetSlide();
}

bool Platform::SafeRead(uintptr_t aAddress, void* aOut, size_t aSize)
{
    mach_vm_size_t read = 0;
    return mach_vm_read_overwrite(mach_task_self(), aAddress, aSize, reinterpret_cast<mach_vm_address_t>(aOut), &read) == KERN_SUCCESS &&
           read == aSize;
}

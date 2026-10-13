#include "Platform.hpp"
#include "Utils.hpp"
#include "App.hpp"

void Platform::ShowMessageBox(const std::string_view aText, bool aIsError)
{
    Utils::ShowMessageBox(Utils::Widen(aText), aIsError ? MB_ICONERROR | MB_OK : MB_ICONWARNING | MB_OK);
}

void Platform::Exit()
{
    TerminateProcess(GetCurrentProcess(), 1);
    std::unreachable();
}

uintptr_t Platform::ResolveAddress(uint64_t aAddress)
{
    return static_cast<uintptr_t>(aAddress) + App::Get()->GetEmpireDllAddr();
}

bool Platform::SafeRead(uintptr_t aAddress, void* aOut, size_t aSize)
{
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(aAddress), aOut, aSize, &read) && read == aSize;
}

#include "Platform.hpp"
#include "Utils.hpp"

void Platform::ShowMessageBox(const std::string_view aText, bool aIsError)
{
    Utils::ShowMessageBox(Utils::Widen(aText), aIsError ? MB_ICONERROR | MB_OK : MB_ICONWARNING | MB_OK);
}

void Platform::Exit()
{
    TerminateProcess(GetCurrentProcess(), 1);
    std::unreachable();
}

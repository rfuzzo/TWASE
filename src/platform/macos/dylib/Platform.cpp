#include "stdafx.hpp"

#include "Platform.hpp"
#include "Utils.hpp"

void Platform::ShowMessageBox(const std::string_view aText, bool aIsError)
{
    Utils::ShowMessageBox(aText, aIsError);
}

void Platform::Exit()
{
    Utils::Exit();
}

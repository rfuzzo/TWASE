#pragma once

// What the shared core needs from the platform, implemented in src/platform/<platform>/.../Platform.cpp

namespace Platform
{
// Shows a message box with UTF-8 text, as an error or a warning.
void ShowMessageBox(const std::string_view aText, bool aIsError);

template<typename... Args>
void ShowMessageBox(bool aIsError, fmt::format_string<Args...> aText, Args&&... aArgs)
{
    ShowMessageBox(fmt::format(aText, std::forward<Args>(aArgs)...), aIsError);
}

// Ends the game right away.
[[noreturn]] void Exit();

// A path as UTF-8, for messages and logs.
inline std::string ToUtf8(const std::filesystem::path& aPath)
{
    const auto text = aPath.u8string();
    return {text.begin(), text.end()};
}
} // namespace Platform

// The message needs at least one argument for its own placeholders.
#ifndef TWASE_MESSAGE_BOX_FILE_LINE
#define TWASE_MESSAGE_BOX_FILE_LINE(msg, ...)                                                                          \
    Platform::ShowMessageBox(false, msg "\n\n{}:{}", __VA_ARGS__, __FILE__, __LINE__)
#endif

#ifndef TWASE_MESSAGE_BOX_AND_EXIT_FILE_LINE
#define TWASE_MESSAGE_BOX_AND_EXIT_FILE_LINE(msg, ...)                                                                 \
    Platform::ShowMessageBox(true, msg "\n\n{}:{}\n\nThe game will close now to prevent unexpected behavior.",        \
                             __VA_ARGS__, __FILE__, __LINE__);                                                         \
    Platform::Exit()
#endif

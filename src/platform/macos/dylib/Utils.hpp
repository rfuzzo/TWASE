#pragma once

class Config;
class Paths;

#ifndef TWASE_UNUSED_PARAMETER
#define TWASE_UNUSED_PARAMETER(param) (void)(param)
#endif

namespace Utils
{
std::shared_ptr<spdlog::logger> CreateLogger(const std::string_view aLogName, const std::string_view aFilename,
                                             const Paths& aPaths, const Config& aConfig);

std::string FormatCurrentTimestamp();

// UTF-16 (game strings) to UTF-8, aLength in code units or npos for null terminated
std::string ToUtf8(const char16_t* aText, size_t aLength = std::string::npos);

void ShowMessageBox(const std::string_view aText, bool aIsError);

template<typename... Args>
void ShowMessageBox(bool aIsError, fmt::format_string<Args...> aText, Args&&... aArgs)
{
    ShowMessageBox(fmt::format(aText, std::forward<Args>(aArgs)...), aIsError);
}

[[noreturn]] void Exit();

bool IsDebuggerPresent();
} // namespace Utils

#ifndef SHOW_MESSAGE_BOX_FILE_LINE
#define SHOW_MESSAGE_BOX_FILE_LINE(msg, ...)                                                                           \
    Utils::ShowMessageBox(false, msg "\n\n{}:{}" __VA_OPT__(, ) __VA_ARGS__, __FILE__, __LINE__)
#endif

#ifndef SHOW_MESSAGE_BOX_AND_EXIT_FILE_LINE
#define SHOW_MESSAGE_BOX_AND_EXIT_FILE_LINE(msg, ...)                                                                  \
    Utils::ShowMessageBox(true, msg "\n\n{}:{}\n\nThe game will close now to prevent unexpected behavior."            \
                                    __VA_OPT__(, ) __VA_ARGS__,                                                        \
                          __FILE__, __LINE__);                                                                         \
    Utils::Exit()
#endif

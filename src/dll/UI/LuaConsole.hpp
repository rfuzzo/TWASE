#pragma once

#include "../../sdk/Attila/Lua/LuaDefs.hpp"

#include <string>
#include <vector>
#include <mutex>

class LuaConsole
{
public:
    enum class LogLevel { Debug, Info, Warn, Error };

    static LuaConsole& Get();

    void Toggle();
    bool IsOpen() const { return m_open; }

    /// Call from the ImGui render loop (inside Present hook)
    void Draw();

    /// Thread-safe: push a log line with an explicit level
    void AddLog(LogLevel level, const char* fmt, ...);

private:
    LuaConsole() = default;

    void AddLogInternal(const char* text, LogLevel level);
    void ExecuteCommand(const char* command);
    bool tryHandleCommand(const std::string& input);

    struct LogEntry {
        std::string text;
        LogLevel    level = LogLevel::Info;
    };

    bool                     m_open = false;
    char                     m_inputBuf[1024] = {};
    std::vector<std::string> m_history;
    std::vector<LogEntry>    m_log;
    std::mutex               m_logMutex;
    bool                     m_scrollToBottom = false;
    int                      m_historyPos = -1;
};

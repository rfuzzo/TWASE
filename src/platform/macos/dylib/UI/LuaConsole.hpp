#pragma once

#include "../../sdk/Attila/Lua/LuaDefs.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

class LuaConsole
{
public:
    enum class LogLevel { Debug, Info, Warn, Error };

    static LuaConsole& Get();

    // thread-safe, the input handler runs on the main thread and the overlay on the render thread
    void Toggle();
    bool IsOpen() const { return m_open; }

    /// Call from the ImGui render loop (inside the Metal present hook)
    void Draw();

    /// Thread-safe: push a log line with an explicit level
    void AddLog(LogLevel level, const char* fmt, ...);

    /// Game thread (GameTick hook): runs the queued commands and refreshes the context list.
    /// Lua is only touched here, the overlay runs on the render thread.
    void ProcessPending();

private:
    LuaConsole() = default;

    void DrawConsoleTab();
    void DrawTweaksTab();

    void DrawContextSwitcher();

    void AddLogInternal(const char* text, LogLevel level);
    void QueueCommand(const std::string& command);
    void ExecuteCommand(const char* command);
    bool tryHandleCommand(const std::string& input);

    struct LogEntry {
        std::string text;
        LogLevel    level = LogLevel::Info;
    };

    std::atomic<bool>        m_open = false;
    char                     m_inputBuf[1024] = {};
    std::vector<std::string> m_history;
    std::vector<LogEntry>    m_log;
    std::mutex               m_logMutex;
    bool                     m_scrollToBottom = false;
    int                      m_historyPos = -1;

    // commands typed on the render thread, executed on the game thread
    std::mutex               m_pendingMutex;
    std::vector<std::string> m_pending;

    // context names cached on the game thread for the overlay
    struct ContextInfo {
        std::string name;
        bool        active = false;
    };
    std::mutex               m_contextsMutex;
    std::vector<ContextInfo> m_contexts;
};

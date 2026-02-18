#pragma once

#include "../../sdk/Attila/Lua/LuaDefs.hpp"

#include <string>
#include <vector>
#include <mutex>

class LuaConsole
{
public:
    static LuaConsole& Get();

    void Toggle();
    bool IsOpen() const { return m_open; }

    /// Call from the ImGui render loop (inside Present hook)
    void Draw();

    /// Thread-safe: push a log line (e.g. from LuaLog hook)
    void AddLog(const char* text);
    void AddLogFmt(const char* fmt, ...);

private:
    LuaConsole() = default;

    void ExecuteCommand(const char* command);
    bool tryHandleCommand(const std::string& input);

    bool                     m_open = false;
    char                     m_inputBuf[1024] = {};
    std::vector<std::string> m_history;
    std::vector<std::string> m_log;
    std::string              m_logBuffer;
    std::mutex               m_logMutex;
    bool                     m_scrollToBottom = false;
    int                      m_historyPos = -1;
};
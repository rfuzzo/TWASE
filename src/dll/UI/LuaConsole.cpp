#include "LuaConsole.hpp"
#include "../Lua/LuaRuntime.hpp"

#include <imgui.h>

LuaConsole& LuaConsole::Get()
{
    static LuaConsole instance;
    return instance;
}

void LuaConsole::Toggle()
{
    m_open = !m_open;
}

void LuaConsole::AddLog(const char* text)
{
    if (!text || text[0] == '\0')
        return;

    std::lock_guard lock(m_logMutex);
    m_log.emplace_back(text);
    m_scrollToBottom = true;
}

void LuaConsole::Draw()
{
    if (!m_open)
        return;

    ImGui::SetNextWindowSize(ImVec2(620, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("TWASE Lua Console", &m_open))
    {
        ImGui::End();
        return;
    }

    // Output region
    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ScrollRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        std::lock_guard lock(m_logMutex);
        for (const auto& line : m_log)
        {
            // Color errors red
            if (line.find("[error]") != std::string::npos || line.find("Error") == 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::TextUnformatted(line.c_str());
            }
        }

        if (m_scrollToBottom)
        {
            ImGui::SetScrollHereY(1.0f);
            m_scrollToBottom = false;
        }
    }
    ImGui::EndChild();

    // Input line
    ImGui::Separator();
    bool reclaimFocus = false;

    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue
                                   | ImGuiInputTextFlags_EscapeClearsAll
                                   | ImGuiInputTextFlags_CallbackHistory;

    auto historyCallback = [](ImGuiInputTextCallbackData* data) -> int
    {
        auto* console = static_cast<LuaConsole*>(data->UserData);
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
        {
            const int prevPos = console->m_historyPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (console->m_historyPos == -1)
                    console->m_historyPos = static_cast<int>(console->m_history.size()) - 1;
                else if (console->m_historyPos > 0)
                    console->m_historyPos--;
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (console->m_historyPos != -1)
                {
                    if (++console->m_historyPos >= static_cast<int>(console->m_history.size()))
                        console->m_historyPos = -1;
                }
            }

            if (prevPos != console->m_historyPos)
            {
                const char* entry = (console->m_historyPos >= 0)
                    ? console->m_history[console->m_historyPos].c_str()
                    : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, entry);
            }
        }
        return 0;
    };

    if (ImGui::InputText("##Input", m_inputBuf, IM_ARRAYSIZE(m_inputBuf), inputFlags, historyCallback, this))
    {
        if (m_inputBuf[0] != '\0')
        {
            ExecuteCommand(m_inputBuf);
            m_history.emplace_back(m_inputBuf);
            m_historyPos = -1;
        }
        m_inputBuf[0] = '\0';
        reclaimFocus = true;
    }

    ImGui::SetItemDefaultFocus();
    if (reclaimFocus)
        ImGui::SetKeyboardFocusHere(-1);

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        std::lock_guard lock(m_logMutex);
        m_log.clear();
    }

    ImGui::End();
}

void LuaConsole::ExecuteCommand(const char* command)
{
    AddLog(fmt::format("> {}", command).c_str());

    if (!LuaRuntime::IsReady())
    {
        AddLog("[error] lua_State not captured yet – game Lua VM hasn't started.");
        return;
    }

    lua_State* L = LuaRuntime::GetState();
    int top = LuaRuntime::gettop(L);

    // Compile
    int loadResult = LuaRuntime::loadstring(L, command);
    if (loadResult != LUA_OK)
    {
        const char* err = LuaRuntime::tolstring(L, -1, nullptr);
        AddLog(fmt::format("[error] {}", err ? err : "unknown compile error").c_str());
        LuaRuntime::settop(L, top); // pop error
        return;
    }

    // Execute
    int callResult = LuaRuntime::pcall(L, 0, LUA_MULTRET, 0);
    if (callResult != LUA_OK)
    {
        const char* err = LuaRuntime::tolstring(L, -1, nullptr);
        AddLog(fmt::format("[error] {}", err ? err : "unknown runtime error").c_str());
        LuaRuntime::settop(L, top); // pop error
        return;
    }

    // Print any return values left on the stack
    int newTop = LuaRuntime::gettop(L);
    for (int i = top + 1; i <= newTop; ++i)
    {
        const char* val = LuaRuntime::tolstring(L, i, nullptr);
        if (val)
            AddLog(val);
    }
    LuaRuntime::settop(L, top); // clean up
}
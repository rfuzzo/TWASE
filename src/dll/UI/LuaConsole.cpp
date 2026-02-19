#include "LuaConsole.hpp"
#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/Lua/LuaGameEnvironment.hpp"

#include <imgui.h>
#include <imgui_internal.h>

LuaConsole& LuaConsole::Get()
{
    static LuaConsole instance;
    return instance;
}

void LuaConsole::Toggle()
{
    m_open = !m_open;
}

void LuaConsole::AddLog(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AddLogInternal(buf);
}

void LuaConsole::AddLogInternal(const char* text)
{
    if (!text || text[0] == '\0')
        return;

    std::lock_guard lock(m_logMutex);
    m_log.emplace_back(text);
    m_scrollToBottom = true;

	spdlog::info("[LuaConsole] {}", text);
}

static void DrawContextSwitcher()
{
    auto contexts = LuaGameEnvironment::GetActiveContexts();
    auto* activeL = LuaGameEnvironment::GetActiveState();

    // Find current index
    int currentIdx = -1;
    for (size_t i = 0; i < contexts.size(); i++) {
        if (contexts[i].L == activeL) { currentIdx = (int)i; break; }
    }

    const char* preview = (currentIdx >= 0) ? contexts[currentIdx].name.c_str() : "Select context...";

    if (ImGui::BeginCombo("##Context", preview)) {
        for (size_t i = 0; i < contexts.size(); i++) {
            bool selected = (contexts[i].L == activeL);
            if (ImGui::Selectable(contexts[i].name.c_str(), selected)) {
                LuaGameEnvironment::SelectContext(i);
            }
        }
        ImGui::EndCombo();
    }
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

    // Output region — InputTextMultiline enables text selection (Ctrl+C etc.)
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

    const ImGuiStyle& style   = ImGui::GetStyle();
    const float clearWidth    = ImGui::CalcTextSize("Clear").x + style.FramePadding.x * 2.0f;
    const float comboWidth    = 200.0f;
    const float spacing       = style.ItemSpacing.x;
    const float inputWidth    = ImGui::GetContentRegionAvail().x - comboWidth - clearWidth - spacing * 2.0f;

    ImGui::SetNextItemWidth(comboWidth);
    DrawContextSwitcher();
    ImGui::SameLine();

    ImGui::SetNextItemWidth(inputWidth);
    if (ImGui::InputText("##Input", m_inputBuf, IM_ARRAYSIZE(m_inputBuf), inputFlags, historyCallback, this))
    {
        if (m_inputBuf[0] != '\0')
        {
            AddLog("> %s", m_inputBuf);
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


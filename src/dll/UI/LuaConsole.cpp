#include "LuaConsole.hpp"
#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/Lua/LuaGameEnvironment.hpp"
#include "../../sdk/Attila/ScriptInterface.hpp"

#include <imgui.h>

#include <sol/sol.hpp>

LuaConsole& LuaConsole::Get()
{
    static LuaConsole instance;
    return instance;
}

void LuaConsole::Toggle()
{
    m_open = !m_open;
}

void LuaConsole::AddLogFmt(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    AddLog(buf);
}

void LuaConsole::AddLog(const char* text)
{
    if (!text || text[0] == '\0')
        return;

    std::lock_guard lock(m_logMutex);
    m_log.emplace_back(text);
    m_scrollToBottom = true;
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

    // Layout: [Ctx combo (fixed)] [InputText (fill)] [Clear (fixed)]
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

bool LuaConsole::tryHandleCommand(const std::string& input)
{
    if (input == ".contexts") {
        auto contexts = LuaGameEnvironment::GetActiveContexts();
        if (contexts.empty()) {
            AddLog("[info] No active lua contexts");
            return true;
        }
        auto* activeL = LuaGameEnvironment::GetActiveState();
        for (size_t i = 0; i < contexts.size(); i++) {
            AddLogFmt("  [%zu] %s%s", i, contexts[i].name.c_str(),
                (contexts[i].L == activeL) ? " (active)" : "");
        }
        return true;
    }

    if (input == ".active") {
        if (LuaGameEnvironment::IsContextValid()) {
            AddLogFmt("[info] %s", LuaGameEnvironment::GetActiveContextName().c_str());
        }
        else {
            AddLog("[info] No active context");
        }
        return true;
    }

    if (input.rfind(".switch ", 0) == 0) {
        std::string arg = input.substr(8);

        // Try as index
        try {
            size_t index = std::stoul(arg);
            if (LuaGameEnvironment::SelectContext(index)) {
                AddLogFmt("[info] Switched to: %s", LuaGameEnvironment::GetActiveContextName().c_str());
            }
            else {
                AddLogFmt("[error] Invalid context index: %zu", index);
            }
            return true;
        }
        catch (...) {}

        // Try as partial name match
        auto contexts = LuaGameEnvironment::GetActiveContexts();
        for (size_t i = 0; i < contexts.size(); i++) {
            if (contexts[i].name.find(arg) != std::string::npos) {
                LuaGameEnvironment::SelectContext(i);
                AddLogFmt("[info] Switched to: %s", LuaGameEnvironment::GetActiveContextName().c_str());
                return true;
            }
        }

        AddLogFmt("[error] No context matching '%s'", arg.c_str());
        return true;
    }

    if (input == ".help") {
        AddLog("Commands:");
        AddLog("  .contexts  - List active lua contexts");
        AddLog("  .active    - Show current context");
        AddLog("  .switch N  - Switch by index or name");
        AddLog("  .help      - Show this help");
        return true;
    }

    return false;
}


void LuaConsole::ExecuteCommand(const char* input)
{
    std::string cmd(input);

    if (cmd[0] == '.') {
        if (tryHandleCommand(cmd)) return;
        AddLogFmt("[error] Unknown command: %s", input);
        return;
    }

    if (!LuaGameEnvironment::IsContextValid()) {
        AddLog("[error] No active lua context");
        return;
    }

    lua_State* L = LuaGameEnvironment::GetActiveState();
    AddLogFmt("> %s", input);

    int top = LuaRuntime::gettop(L);

    if (LuaRuntime::loadbuffer(L, cmd.c_str(), cmd.size(), "console") != 0) {
        AddLogFmt("[error] %s", LuaRuntime::tolstring(L, -1, nullptr));
        LuaRuntime::settop(L, top);
        return;
    }

    if (LuaRuntime::pcall(L, 0, LUA_MULTRET, 0) != 0) {
        AddLogFmt("[error] %s", LuaRuntime::tolstring(L, -1, nullptr));
        LuaRuntime::settop(L, top);
        return;
    }

    // Print any return values
    int nresults = LuaRuntime::gettop(L) - top;
    for (int i = 1; i <= nresults; i++) {
        const char* str = LuaRuntime::tolstring(L, top + i, nullptr);
        if (str) {
            AddLogFmt("%s", str);
        }
    }

    LuaRuntime::settop(L, top);
}
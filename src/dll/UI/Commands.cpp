#include "LuaConsole.hpp"

#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/Lua/LuaGameEnvironment.hpp"

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

    if (input == ".globals") {
        if (!LuaGameEnvironment::IsContextValid()) {
            AddLog("[error] No active context");
            return true;
        }
        auto entries = LuaGameEnvironment::GetTableEntries(
            LuaGameEnvironment::GetActiveState(), "", true);
        AddLogFmt("[info] %zu globals:", entries.size());
        for (auto& e : entries) {
            AddLog(("  " + e).c_str());
        }
        return true;
    }

    if (input.rfind(".list ", 0) == 0) {
        if (!LuaGameEnvironment::IsContextValid()) {
            AddLog("[error] No active context");
            return true;
        }
        std::string table = input.substr(6);
        auto entries = LuaGameEnvironment::GetTableEntries(
            LuaGameEnvironment::GetActiveState(), table, true);
        if (entries.empty()) {
            AddLogFmt("[info] '%s' has no fields or is not a table/userdata", table.c_str());
        }
        else {
            AddLogFmt("[info] %zu fields in '%s':", entries.size(), table.c_str());
            for (auto& e : entries) {
                AddLog(("  " + e).c_str());
            }
        }
        return true;
    }

    if (input == ".help") {
        AddLog("Commands:");
        AddLog("  .contexts        - List active lua contexts");
        AddLog("  .active          - Show current context");
        AddLog("  .switch N        - Switch by index or name");
        AddLog("  .globals         - List all globals in current context");
        AddLog("  .list <table>    - List fields of a table (e.g. .list CampaignUI)");
        AddLog("  .help            - Show this help");
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
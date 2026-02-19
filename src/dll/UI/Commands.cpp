#include "LuaConsole.hpp"

#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/Lua/LuaGameEnvironment.hpp"

bool LuaConsole::tryHandleCommand(const std::string& input)
{
    if (input == ".contexts") {
        auto contexts = LuaGameEnvironment::GetActiveContexts();
        if (contexts.empty()) {
            AddLog(LogLevel::Info, "No active lua contexts");
            return true;
        }
        auto* activeL = LuaGameEnvironment::GetActiveState();
        for (size_t i = 0; i < contexts.size(); i++) {
            AddLog(LogLevel::Info, "  [%zu] %s%s", i, contexts[i].name.c_str(),
                (contexts[i].L == activeL) ? " (active)" : "");
        }
        return true;
    }

    if (input == ".active") {
        if (LuaGameEnvironment::IsContextValid()) {
            AddLog(LogLevel::Info, "%s", LuaGameEnvironment::GetActiveContextName().c_str());
        }
        else {
            AddLog(LogLevel::Info, "No active context");
        }
        return true;
    }

    if (input.rfind(".switch ", 0) == 0) {
        std::string arg = input.substr(8);

        // Try as index
        try {
            size_t index = std::stoul(arg);
            if (LuaGameEnvironment::SelectContext(index)) {
                AddLog(LogLevel::Info, "Switched to: %s", LuaGameEnvironment::GetActiveContextName().c_str());
            }
            else {
                AddLog(LogLevel::Error, "Invalid context index: %zu", index);
            }
            return true;
        }
        catch (...) {}

        // Try as partial name match
        auto contexts = LuaGameEnvironment::GetActiveContexts();
        for (size_t i = 0; i < contexts.size(); i++) {
            if (contexts[i].name.find(arg) != std::string::npos) {
                LuaGameEnvironment::SelectContext(i);
                AddLog(LogLevel::Info, "Switched to: %s", LuaGameEnvironment::GetActiveContextName().c_str());
                return true;
            }
        }

        AddLog(LogLevel::Error, "No context matching '%s'", arg.c_str());
        return true;
    }

    if (input == ".globals") {
        if (!LuaGameEnvironment::IsContextValid()) {
            AddLog(LogLevel::Error, "No active context");
            return true;
        }
        auto entries = LuaGameEnvironment::GetTableEntries(
            LuaGameEnvironment::GetActiveState(), "", true);
        AddLog(LogLevel::Info, "%zu globals:", entries.size());
        for (auto& e : entries) {
            AddLog(LogLevel::Info, "  %s", e.c_str());
        }
        return true;
    }

    if (input.rfind(".list ", 0) == 0) {
        if (!LuaGameEnvironment::IsContextValid()) {
            AddLog(LogLevel::Error, "No active context");
            return true;
        }
        std::string table = input.substr(6);
        auto entries = LuaGameEnvironment::GetTableEntries(
            LuaGameEnvironment::GetActiveState(), table, true);
        if (entries.empty()) {
            AddLog(LogLevel::Info, "'%s' has no fields or is not a table/userdata", table.c_str());
        }
        else {
            AddLog(LogLevel::Info, "%zu fields in '%s':", entries.size(), table.c_str());
            for (auto& e : entries) {
                AddLog(LogLevel::Info, "  %s", e.c_str());
            }
        }
        return true;
    }

    if (input == ".help") {
        AddLog(LogLevel::Info, "Commands:");
        AddLog(LogLevel::Info, "  .contexts        - List active lua contexts");
        AddLog(LogLevel::Info, "  .active          - Show current context");
        AddLog(LogLevel::Info, "  .switch N        - Switch by index or name");
        AddLog(LogLevel::Info, "  .globals         - List all globals in current context");
        AddLog(LogLevel::Info, "  .list <table>    - List fields of a table (e.g. .list CampaignUI)");
        AddLog(LogLevel::Info, "  .help            - Show this help");
        return true;
    }

    return false;
}


void LuaConsole::ExecuteCommand(const char* input)
{
    std::string cmd(input);

    if (cmd[0] == '.') {
        if (tryHandleCommand(cmd)) return;
        AddLog(LogLevel::Error, "Unknown command: %s", input);
        return;
    }

    if (!LuaGameEnvironment::IsContextValid()) {
        AddLog(LogLevel::Error, "No active lua context");
        return;
    }

    lua_State* L = LuaGameEnvironment::GetActiveState();

    int top = LuaRuntime::gettop(L);

    if (LuaRuntime::loadbuffer(L, cmd.c_str(), cmd.size(), "console") != 0) {
        AddLog(LogLevel::Error, "%s", LuaRuntime::tolstring(L, -1, nullptr));
        LuaRuntime::settop(L, top);
        return;
    }

    if (LuaRuntime::pcall(L, 0, LUA_MULTRET, 0) != 0) {
        AddLog(LogLevel::Error, "%s", LuaRuntime::tolstring(L, -1, nullptr));
        LuaRuntime::settop(L, top);
        return;
    }

    // Print any return values
    int nresults = LuaRuntime::gettop(L) - top;
    for (int i = 1; i <= nresults; i++) {
        const char* str = LuaRuntime::tolstring(L, top + i, nullptr);
        if (str) {
            AddLog(LogLevel::Info, "%s", str);
        }
    }

    LuaRuntime::settop(L, top);
}

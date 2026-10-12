#pragma once

#include "LuaDefs.hpp"
#include "../ScriptInterface.hpp"

#include <atomic>
#include <string>
#include <vector>

using namespace sdk::Attila;

struct LuaContext
{
    lua_State* L;
    std::string name;
};

class LuaGameEnvironment
{
    using GetLuaState_t = lua_State* (*)(void* thisPtr);

public:
    // aSlide: ASLR slide of the game executable
    static void Init(uintptr_t aSlide);
    static bool IsReady()
    {
        return s_ready.load(std::memory_order_acquire);
    }

    static std::vector<LuaContext> GetActiveContexts();
    static lua_State* GetActiveState();
    static const std::string& GetActiveContextName();
    static bool SelectContext(size_t index);
    static bool IsContextValid();

    static std::vector<std::string> GetTableEntries(lua_State* L, const std::string& tablePath, bool showTypes);

public:
    static lua_State* GetLuaState(void* scriptInterface);

private:
    static std::atomic<bool> s_ready;

    static RuntimeLuaNode** s_listHead;
    static RuntimeLuaNode* s_sentinel;
    static lua_State* s_activeL;
    static std::string s_activeContextName;

    static GetLuaState_t s_GetLuaState;
};

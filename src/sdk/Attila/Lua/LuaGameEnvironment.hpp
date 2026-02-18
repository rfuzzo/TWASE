#pragma once

#include "LuaDefs.hpp"
#include "../ScriptInterface.hpp"
#include <atomic>
#include <string>
#include <vector>

using namespace sdk::Attila;

struct LuaContext {
    lua_State* L;
    std::string  name;
};

class LuaGameEnvironment
{
public:
    static void Init(uint32_t empireDllBase);
    static bool IsReady();

    static std::vector<LuaContext>  GetActiveContexts();
    static lua_State*               GetActiveState();
    static const std::string&       GetActiveContextName();
    static bool                     SelectContext(size_t index);
    static bool                     IsContextValid();
private:
    static RuntimeLuaNode** s_listHead;
    static RuntimeLuaNode*  s_sentinel;
    static lua_State*       s_activeL;
    static std::string      s_activeContextName;

    static std::atomic<bool>   s_ready;
};
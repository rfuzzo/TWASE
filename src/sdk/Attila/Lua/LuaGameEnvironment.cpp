#include "LuaGameEnvironment.hpp"
#include "../Addresses.hpp"
#include "LuaRuntime.hpp"

#include <spdlog/spdlog.h>

using namespace sdk::Attila;

// static member definitions
RuntimeLuaNode **LuaGameEnvironment::s_listHead = nullptr;
RuntimeLuaNode *LuaGameEnvironment::s_sentinel = nullptr;
lua_State *LuaGameEnvironment::s_activeL = nullptr;
std::string LuaGameEnvironment::s_activeContextName;
std::atomic<bool> LuaGameEnvironment::s_ready = false;

void LuaGameEnvironment::Init(uint32_t base)
{
    s_listHead = reinterpret_cast<RuntimeLuaNode **>(base + Addresses::g_RuntimeLuaListHead);
    s_sentinel = reinterpret_cast<RuntimeLuaNode *>(base + Addresses::g_RuntimeLuaListSentinel);

    spdlog::info("LuaGameEnvironment resolved all function pointers");

    s_ready.store(true, std::memory_order_release);
}

std::vector<LuaContext> LuaGameEnvironment::GetActiveContexts()
{
    std::vector<LuaContext> contexts;

    RuntimeLuaNode *node = *s_listHead;
    RuntimeLuaNode *sentinel = s_sentinel;

    while (node && node != sentinel)
    {
        if (node->runtime && node->runtime->L)
        {
            lua_State *L = node->runtime->L;

            std::string name = "unknown";
            LuaRuntime::getfield(L, LUA_GLOBALSINDEX, "decoda_name");
            size_t len = 0;
            const char *str = LuaRuntime::tolstring(L, -1, &len);
            if (str)
            {
                name = std::string(str, len);
            }
            LuaRuntime::settop(L, -2);

            contexts.push_back({L, name});
        }
        node = node->next;
    }

    return contexts;
}

lua_State *LuaGameEnvironment::GetActiveState()
{
    return s_activeL;
}

const std::string &LuaGameEnvironment::GetActiveContextName()
{
    return s_activeContextName;
}

bool LuaGameEnvironment::SelectContext(size_t index)
{
    auto contexts = GetActiveContexts();
    if (index >= contexts.size())
        return false;

    s_activeL = contexts[index].L;
    s_activeContextName = contexts[index].name;
    return true;
}

bool LuaGameEnvironment::IsContextValid()
{
    if (!s_activeL)
        return false;

    for (const auto &ctx : GetActiveContexts())
    {
        if (ctx.L == s_activeL)
            return true;
    }
    return false;
}

bool LuaGameEnvironment::IsReady()
{
    return s_ready.load(std::memory_order_acquire);
}
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

    RuntimeLuaNode* node = *s_listHead;
    RuntimeLuaNode* sentinel = s_sentinel;

    while (node && node != sentinel)
    {
        if (node->runtime && node->runtime->L)
        {
            lua_State* L = node->runtime->L;

            std::string name = "unknown";
            LuaRuntime::getfield(L, LUA_GLOBALSINDEX, "decoda_name");
            size_t len = 0;
            const char* str = LuaRuntime::tolstring(L, -1, &len);
            if (str)
            {
                name = std::string(str, len);
            }
            LuaRuntime::settop(L, -2);

            contexts.push_back({ L, name });
        }
        node = node->next;
    }

    // Validate current selection is still alive
    if (s_activeL != nullptr)
    {
        bool found = false;
        for (auto& ctx : contexts)
        {
            if (ctx.L == s_activeL) { found = true; break; }
        }
        if (!found)
        {
            s_activeL = nullptr;
            s_activeContextName = "none";
        }
    }

    // Auto-select if nothing active
    if (s_activeL == nullptr && !contexts.empty())
    {
        s_activeL = contexts[0].L;
        s_activeContextName = contexts[0].name;
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

std::vector<std::string> LuaGameEnvironment::GetTableEntries(lua_State* L, const std::string& tablePath, bool showTypes)
{
    std::vector<std::string> results;
    int top = LuaRuntime::gettop(L);

    // Navigate to the table: "FrontEnd.Something.Deeper"
    if (tablePath.empty())
    {
        LuaRuntime::pushvalue(L, LUA_GLOBALSINDEX);
    }
    else
    {
        LuaRuntime::pushvalue(L, LUA_GLOBALSINDEX);

        size_t start = 0;
        while (start < tablePath.size())
        {
            size_t dot = tablePath.find('.', start);
            if (dot == std::string::npos) dot = tablePath.size();

            std::string key = tablePath.substr(start, dot - start);
            LuaRuntime::getfield(L, -1, key.c_str());
            LuaRuntime::remove(L, -2); // remove parent

            if (LuaRuntime::type(L, -1) == LUA_TNIL)
            {
                LuaRuntime::settop(L, top);
                return results;
            }

            start = dot + 1;
        }
    }

    // If it's userdata, try its metatable.__index
    if (LuaRuntime::type(L, -1) == LUA_TUSERDATA)
    {
        if (LuaRuntime::getmetatable(L, -1))
        {
            LuaRuntime::getfield(L, -1, "__index");
            LuaRuntime::remove(L, -2); // remove metatable
            LuaRuntime::remove(L, -2); // remove userdata
        }
        else
        {
            LuaRuntime::settop(L, top);
            return results;
        }
    }

    if (LuaRuntime::type(L, -1) != LUA_TTABLE)
    {
        LuaRuntime::settop(L, top);
        return results;
    }

    // Iterate
    LuaRuntime::pushnil(L);
    while (LuaRuntime::next(L, -2) != 0)
    {
        if (LuaRuntime::type(L, -2) == LUA_TSTRING)
        {
            const char* key = LuaRuntime::tolstring(L, -2, nullptr);
            if (key)
            {
                if (showTypes)
                {
                    const char* typeName = "";
                    switch (LuaRuntime::type(L, -1))
                    {
                    case LUA_TFUNCTION: typeName = "function"; break;
                    case LUA_TTABLE:    typeName = "table";    break;
                    case LUA_TSTRING:   typeName = "string";   break;
                    case LUA_TNUMBER:   typeName = "number";   break;
                    case LUA_TBOOLEAN:  typeName = "boolean";  break;
                    case LUA_TUSERDATA: typeName = "userdata"; break;
                    default:            typeName = "other";    break;
                    }
                    results.push_back(std::string(key) + "  (" + typeName + ")");
                }
                else
                {
                    results.push_back(key);
                }
            }
        }
        LuaRuntime::settop(L, -2); // pop value, keep key
    }

    LuaRuntime::settop(L, top);
    std::sort(results.begin(), results.end());
    return results;
}
#pragma once

#include "Lua/LuaDefs.hpp"
#include "VFSTypes.hpp"

#include <cstddef>

namespace sdk::Attila
{
// created lazily by GetScriptRuntime (0x1038b3794)
struct ScriptRuntime
{
    lua_State* L;           // +0x00
    bool initialized;       // +0x08
    void* scriptInterface;  // +0x10
    uint64_t owned;         // +0x18
    uint64_t field_20;      // +0x20
    uint64_t field_28;      // +0x28
};
static_assert(sizeof(ScriptRuntime) == 0x30);

// UTILITYDLL::LUA::LuaEnv (RTTI), base of all game Lua environments, ctor 0x1038b3690.
// Virtual slot 3 (pure) loads and runs the environment's startup script.
struct ScriptInterface
{
    void* vtable;                          // +0x00
    ScriptRuntime* runtime;                // +0x08
    lua_State** parentState;               // +0x10
    uint64_t field_18;                     // +0x18
    void (*registerBindings)(lua_State*);  // +0x20
    uint32_t owned;                        // +0x28
};
static_assert(offsetof(ScriptInterface, registerBindings) == 0x20);
static_assert(offsetof(ScriptInterface, owned) == 0x28);

// appended to g_RuntimeLuaListHead by RegisterScriptRuntime (0x1038b261c)
struct RuntimeLuaNode
{
    RuntimeLuaNode* prev;    // +0x00
    RuntimeLuaNode* next;    // +0x08
    ScriptRuntime* runtime;  // +0x10
    lua_State* baseL;        // +0x18
    int registry_ref;        // +0x20
};
static_assert(sizeof(RuntimeLuaNode) == 0x28);

// EMPIRECAMPAIGN::EPISODIC_SCRIPTING_ENV (RTTI) : UTILITYDLL::LUA::LuaEnv, EMPIREUTILITY::KEY_CONTROLLED
// The campaign scripting environment, ctor 0x10216fe40, RunStartupPath is its virtual slot 3 (0x102170104).
// Same layout as on Windows with 8 byte pointers (there the two paths were labeled the other way round).
struct ScriptingEnv
{
    ScriptInterface base;     // +0x00 LuaEnv
    char keyControlled[0x28]; // +0x30 KEY_CONTROLLED, has its own vtable
    TempString folderPath;    // +0x58 "campaigns/main_attila"
    TempString scriptPath;    // +0x68 "campaigns/main_attila/scripting.lua"
};
static_assert(offsetof(ScriptingEnv, folderPath) == 0x58);
static_assert(offsetof(ScriptingEnv, scriptPath) == 0x68);
static_assert(sizeof(ScriptingEnv) == 0x78);

} // namespace sdk::Attila

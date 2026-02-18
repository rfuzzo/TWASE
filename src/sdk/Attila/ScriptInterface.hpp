#pragma once

#include "Lua/LuaDefs.hpp"

namespace sdk::Attila
{
    struct ScriptRuntime {
        lua_State* L;                  // +0x00
        bool            initialized;        // +0x04
        char            _pad[3];
        void* scriptInterface;    // +0x08
        bool            owned;              // +0x0C
        char            _pad2[3];
        uint32_t        field_10;           // +0x10
        uint32_t        field_14;           // +0x14
    };

    struct ScriptInterface {
        void* vtable;             // +0x00
        ScriptRuntime* runtime;            // +0x04
        lua_State** parentState;        // +0x08
        bool            field_0C;           // +0x0C
        char            _pad[3];
        void (*registerBindings)(lua_State*); // +0x10
        bool            owned;              // +0x14
        char            _pad2[3];
    };

    struct RuntimeLuaNode {
        RuntimeLuaNode* prev;
        RuntimeLuaNode* next;
        ScriptRuntime*  runtime;
        lua_State*      baseL;              
        int             registry_ref;       
                   
    };

} // namespace sdk::Attila

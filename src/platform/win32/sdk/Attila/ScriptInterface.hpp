#pragma once

#include "Lua/LuaDefs.hpp"
#include "VFSTypes.hpp"

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

    struct ScriptingEnv {
        // Base ScriptInterface (0x18 bytes)
        void* vtable;
        void* runtime;
        lua_State** parentState;
        bool   field_0C;
        char   _pad[3];
        void   (*registerBindings)(lua_State*);
        bool   owned;
        char   _pad2[3];
        // Sub-object at +0x18 (0x1C bytes, has its own vtable)
        char   subObject[0x1C];
        // Script path at +0x34
        TempString scriptPath;
        // Folder path at +0x40
        TempString folderPath;
    };

} // namespace sdk::Attila

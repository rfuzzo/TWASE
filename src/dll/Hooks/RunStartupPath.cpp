#include "RunStartupPath.hpp"

#include "../App.hpp"
#include "../Config.hpp"
#include "../UI/LuaConsole.hpp"
#include "../Hooking/Hook.hpp"

#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/Lua/LuaGameEnvironment.hpp"
#include "../../sdk/Attila/VFS/VFS.hpp"

#include "../../sdk/Attila/Addresses.hpp" 
#include "../../sdk/Attila/ScriptInterface.hpp" 

// empire.dll uses a custom luaBPrint function to log lua messages, it is essentially "fputs or custom sink".
// We hook this and just add our spdlog to it, and pass the buffer back to the original.

namespace
{
bool isAttached = false;

void RunStartupPath(sdk::Attila::ScriptingEnv* self);
Hook<decltype(&RunStartupPath)> RunStartupPathHook_fnc(sdk::Attila::Addresses::RunStartupPath, &RunStartupPath);

static std::vector<std::string> EnumerateModFolders(const std::string campaignFolder)
{
    std::vector<std::string> mods;

    void* vfs = VFS::VFSGetInstance();
    if (!vfs) return mods;

    char searchDir[MAX_PATH];
    snprintf(searchDir, MAX_PATH, "%s/mods", campaignFolder.c_str());

    VFSSearchResults results = {};
    
    CName dirName = {};
    CName patternName = {};
    VFS::CName_ctor(&dirName, searchDir);
    VFS::CName_ctor(&patternName, "scripting.lua");

    VFS::VFSSearchFiles(vfs, dirName.pooled, patternName.pooled, &results, 1, 3);

    if (results.count <= 0) return mods;

    spdlog::info("[ModLoader] VFS search returned {} result(s)", results.count);

    for (int i = 0; i < results.count; i++) {
        uint32_t entry = (uint32_t)results.entries[i];
        const char* path = *(const char**)(entry + 8);
        if (!path) continue;
        spdlog::info("[ModLoader] Found: {}", path);

        // Skip if there's no subdirectory (i.e. it's mods/scripting.lua directly)
        std::string pathStr(path);
        size_t modsPos = pathStr.find("mods\\");
        if (modsPos == std::string::npos) continue;

        std::string afterMods = pathStr.substr(modsPos + 5); // everything after "mods/"
        if (afterMods == "scripting.lua") continue; // skip top-level mods/scripting.lua

        // Extract mod name from "rfmod/scripting.lua"
        size_t slash = afterMods.find('\\');
        if (slash == std::string::npos) continue;
        std::string modName = afterMods.substr(0, slash);

        if (!modName.empty() && modName != "mods") {
            spdlog::info("[ModLoader] Found mod: {} (path: {})", modName, path);
            mods.push_back(modName);
        }
    }

    // free
    if (results.entries) {
        VFS::tw_free(results.entries);
    }

    std::sort(mods.begin(), mods.end());
    return mods;
}

void RunStartupPath(sdk::Attila::ScriptingEnv* self)
{
    RunStartupPathHook_fnc(self);

    if (App::Get() && App::Get()->GetConfig() && App::Get()->GetConfig()->GetScripting().autoLoadMods)
    {
        lua_State* L = LuaGameEnvironment::GetLuaState(self);
        if (!L) return;

        // Enumerate mods via VFS
        // get folder from file (campaigns/main_attila/scripting.lua)
        auto campaignScriptingFile = self->folderPath.data;
        auto lastSlash = std::string(campaignScriptingFile).find_last_of("/\\");
        if (lastSlash == std::string::npos)
        {
            return;
        }
        std::string campaignFolder = std::string(campaignScriptingFile).substr(0, lastSlash);
        auto mods = EnumerateModFolders(campaignFolder);

        int loaded = 0, failed = 0;
        for (const auto& mod : mods) {
            char code[512];
            snprintf(code, sizeof(code), "require('mods/%s/scripting')", mod.c_str());

            spdlog::info("[ModLoader] Loading: {}", mod);

            if (LuaRuntime::loadbuffer(L, code, strlen(code), mod.c_str()) != 0 || LuaRuntime::pcall(L, 0, 0, 0) != 0) {
                const char* err = LuaRuntime::tolstring(L, -1, nullptr);
                spdlog::error("[ModLoader] Error in {}: {}", mod, err ? err : "unknown");
                LuaRuntime::settop(L, -2);
                failed++;
            }
            else {
                spdlog::info("[ModLoader] Loaded: {}", mod);
                loaded++;
            }
        }

        spdlog::info("[ModLoader] Finished: {} loaded, {} failed", loaded, failed);
    }
}
} // namespace

bool Hooks::RunStartupPathHook::Attach()
{
    spdlog::trace("Trying to attach the hook for runstartup at {:#x}...", RunStartupPathHook_fnc.GetAddress());

    auto result = RunStartupPathHook_fnc.Attach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not attach the hook for runstartup. Detour error code: {}", result);
    }
    else
    {
        spdlog::info("The hook for runstartup was attached");
    }

    isAttached = result == NO_ERROR;
    return isAttached;
}

bool Hooks::RunStartupPathHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }

    spdlog::trace("Trying to detach the hook for runstartup at {:#x}...", RunStartupPathHook_fnc.GetAddress());

    auto result = RunStartupPathHook_fnc.Detach();
    if (result != NO_ERROR)
    {
        spdlog::error("Could not detach the hook for runstartup. Detour error code: {}", result);
    }
    else
    {
        spdlog::trace("The hook for runstartup was detached");
    }

    isAttached = result != NO_ERROR;
    return !isAttached;
}

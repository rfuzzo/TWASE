#include "stdafx.hpp"

#include "RunStartupPath.hpp"

#include "../App.hpp"
#include "../Config.hpp"
#include "../Hooking/Hook.hpp"
#include "../Scripting/LuaApi.hpp"

#include "../../sdk/Attila/Addresses.hpp"
#include "../../sdk/Attila/ScriptInterface.hpp"
#include "../../sdk/Attila/Lua/LuaGameEnvironment.hpp"
#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/VFS/VFS.hpp"

// After the campaign's scripting.lua ran, register the twase Lua table, then load every <campaign folder>/mods/<name>/scripting.lua the game's VFS can see
// (inside packs or as loose files in TotalWarAttilaData/data), sorted by name.

namespace
{
bool isAttached = false;

// EPISODIC_SCRIPTING_ENV virtual slot 3, runs on the WinMain (game Lua) thread
void* RunStartupPath(sdk::Attila::ScriptingEnv* self);
void LoadMods(sdk::Attila::ScriptingEnv* self);
Hook<decltype(&RunStartupPath)> RunStartupPathHook_fnc(sdk::Attila::Addresses::RunStartupPath, &RunStartupPath);

std::vector<std::string> EnumerateModFolders(const std::string& campaignFolder)
{
    std::vector<std::string> mods;

    void* vfs = VFS::VFSGetInstance();
    if (!vfs) return mods;

    auto searchDir = campaignFolder + "/mods";

    VFSSearchResults results = {};

    CName dirName = {};
    CName patternName = {};
    VFS::CName_ctor(&dirName, searchDir.c_str());
    VFS::CName_ctor(&patternName, "scripting.lua");

    // flags 1: include sub folders
    VFS::VFSSearchFiles(vfs, &dirName, &patternName, &results, 1, 3);

    spdlog::info("[ModLoader] VFS search in '{}' returned {} result(s)", searchDir, results.count);

    for (uint32_t i = 0; i < results.count; i++)
    {
        const VFSEntry* entry = results.entries ? results.entries[i] : nullptr;
        const char* path = entry ? entry->path : nullptr;
        if (!path) continue;
        spdlog::debug("[ModLoader] Found: {}", path);

        // the VFS uses backslashes, accept both
        std::string pathStr(path);
        std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

        size_t modsPos = pathStr.find("mods/");
        if (modsPos == std::string::npos) continue;

        std::string afterMods = pathStr.substr(modsPos + 5); // everything after "mods/"
        if (afterMods == "scripting.lua") continue;          // skip top-level mods/scripting.lua

        // Extract mod name from "rfmod/scripting.lua", only direct sub folders count
        size_t slash = afterMods.find('/');
        if (slash == std::string::npos || afterMods.substr(slash + 1) != "scripting.lua") continue;
        std::string modName = afterMods.substr(0, slash);

        if (!modName.empty() && modName != "mods" && std::find(mods.begin(), mods.end(), modName) == mods.end())
        {
            spdlog::info("[ModLoader] Found mod: {} (path: {})", modName, path);
            mods.push_back(modName);
        }
    }

    // free
    if (results.entries)
    {
        VFS::tw_free(results.entries);
    }

    std::sort(mods.begin(), mods.end());
    return mods;
}

void* RunStartupPath(sdk::Attila::ScriptingEnv* self)
{
    auto result = RunStartupPathHook_fnc(self);

    // the twase table, before the mods so they can use it while loading
    if (lua_State* L = LuaGameEnvironment::GetLuaState(self))
    {
        Scripting::LuaApi::Register(L);
    }

    LoadMods(self);
    return result;
}

void LoadMods(sdk::Attila::ScriptingEnv* self)
{
    if (!App::Get() || !App::Get()->GetConfig()->GetScripting().autoLoadMods)
    {
        return;
    }

    lua_State* L = LuaGameEnvironment::GetLuaState(self);
    if (!L) return;

    // get folder from file (campaigns/main_attila/scripting.lua)
    const char* campaignScriptingFile = self->scriptPath.data;
    if (!campaignScriptingFile) return;

    std::string scriptingFile(campaignScriptingFile);
    auto lastSlash = scriptingFile.find_last_of("/\\");
    if (lastSlash == std::string::npos)
    {
        return;
    }

    std::string campaignFolder = scriptingFile.substr(0, lastSlash);
    spdlog::debug("[ModLoader] Campaign script: {} (folder field: {})", scriptingFile,
                  self->folderPath.data ? self->folderPath.data : "(null)");

    auto mods = EnumerateModFolders(campaignFolder);

    int loaded = 0, failed = 0;
    for (const auto& mod : mods)
    {
        auto code = fmt::format("require('mods/{}/scripting')", mod);

        spdlog::info("[ModLoader] Loading: {}", mod);

        if (LuaRuntime::loadbuffer(L, code.c_str(), code.size(), mod.c_str()) != 0 || LuaRuntime::pcall(L, 0, 0, 0) != 0)
        {
            const char* err = LuaRuntime::tolstring(L, -1, nullptr);
            spdlog::error("[ModLoader] Error in {}: {}", mod, err ? err : "unknown");
            LuaRuntime::settop(L, -2);
            failed++;
        }
        else
        {
            spdlog::info("[ModLoader] Loaded: {}", mod);
            loaded++;
        }
    }

    spdlog::info("[ModLoader] Finished: {} loaded, {} failed", loaded, failed);
}
} // namespace

bool Hooks::RunStartupPathHook::Attach()
{
    spdlog::trace("Trying to attach the hook for runstartup at {:#x}...", RunStartupPathHook_fnc.GetAddress());

    auto result = RunStartupPathHook_fnc.Attach();
    if (result != 0)
    {
        spdlog::error("Could not attach the hook for runstartup. Dobby error code: {}", result);
    }
    else
    {
        spdlog::info("The hook for runstartup was attached");
    }

    isAttached = result == 0;
    return isAttached;
}

bool Hooks::RunStartupPathHook::Detach()
{
    if (!isAttached)
    {
        return false;
    }

    auto result = RunStartupPathHook_fnc.Detach();
    isAttached = result != 0;
    return !isAttached;
}

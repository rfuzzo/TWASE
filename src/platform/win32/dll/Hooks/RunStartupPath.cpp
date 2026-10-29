#include "RunStartupPath.hpp"

#include "../App.hpp"
#include "Config.hpp"
#include "UI/LuaConsole.hpp"
#include "../Hooking/Hook.hpp"
#include "Scripting/LuaApi.hpp"

#include "../../sdk/Attila/Lua/LuaRuntime.hpp"
#include "../../sdk/Attila/Lua/LuaGameEnvironment.hpp"
#include "../../sdk/Attila/VFS/VFS.hpp"

#include "../../sdk/Attila/Addresses.hpp" 
#include "../../sdk/Attila/ScriptInterface.hpp" 

namespace
{
bool isAttached = false;

// __thiscall in the game (this in ecx, no stack args), hooked as __fastcall with an unused edx
int __fastcall RunStartupPath(sdk::Attila::ScriptingEnv* self, void* edx);
void LoadMods(sdk::Attila::ScriptingEnv* self);
Hook<decltype(&RunStartupPath)> RunStartupPathHook_fnc(sdk::Attila::Addresses::RunStartupPath, &RunStartupPath);

static std::vector<std::string> EnumerateModFolders(const std::string& campaignFolder)
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
    VFS::VFSSearchFiles(vfs, dirName.pooled, patternName.pooled, &results, 1, 3);

    spdlog::info("[ModLoader] VFS search in '{}' returned {} result(s)", searchDir, results.count);

    for (unsigned int i = 0; i < results.count; i++)
    {
        uint32_t entry = results.entries ? (uint32_t)results.entries[i] : 0;
        const char* path = entry ? *(const char**)(entry + 8) : nullptr;
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

    // the loose mods, which the VFS search misses next to a pack with the same mods folder
    if (App::Get())
    {
        const auto looseDir = App::Get()->GetPaths()->GetRootDir() / "data" / campaignFolder / "mods";
        std::error_code err;
        for (const auto& entry : std::filesystem::directory_iterator(looseDir, err))
        {
            std::error_code fileErr;
            const std::string modName = entry.path().filename().string();
            if (!entry.is_directory(fileErr) || !std::filesystem::is_regular_file(entry.path() / "scripting.lua", fileErr) ||
                std::find(mods.begin(), mods.end(), modName) != mods.end())
            {
                continue;
            }
            spdlog::info("[ModLoader] Found loose mod: {} (path: {})", modName, (entry.path() / "scripting.lua").string());
            mods.push_back(modName);
        }
    }

    std::sort(mods.begin(), mods.end());
    return mods;
}

int __fastcall RunStartupPath(sdk::Attila::ScriptingEnv* self, void* edx)
{
    auto result = RunStartupPathHook_fnc(self, edx);

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
    if (App::Get() && App::Get()->GetConfig() && App::Get()->GetConfig()->GetScripting().autoLoadMods)
    {
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

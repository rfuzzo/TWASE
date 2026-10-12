-- Windows: TWASE.dll and the winmm.dll proxy loader (32-bit, Attila.exe)

-- Hardcoded game root for post-build deployment (can be overridden with TWASE_GAMEROOT env var)
-- Note: forward slashes are fine on Windows; adjust to your install if needed.
local gameroot = os.getenv("TWASE_GAMEROOT") or [[D:/SteamLibrary/steamapps/common/Total War Attila]]

add_requires("wil", "fmt", "spdlog", "toml11", "ordered_map", "microsoft-detours")
-- add_requires("sol2", "lua") -- v5.1.5
add_requires("imgui v1.91.8-docking", {configs = {dx11 = true, win32 = true}})

-- global settings
add_defines(
        "WINVER=0x0601",
        --"_WIN32_WINNT=0x0601",
        "UNICODE", 
        "_UNICODE", 
        "WIN32_LEAN_AND_MEAN",
        "NOMINMAX",

        "SPDLOG_WCHAR_TO_UTF8_SUPPORT", "SPDLOG_WCHAR_FILENAMES", "SPDLOG_WCHAR_SUPPORT"
        )

add_syslinks("User32", "shell32", "ole32", "version")

target("loader")
    set_basename("winmm") 
    set_kind("shared")
    
    -- files
    add_files("loader/*.cpp")    
    add_headerfiles("loader/*.hpp")

    add_files("loader/Proxies/*.cpp")
    add_headerfiles("loader/Proxies/*.hpp")

    add_files("loader/*.rc")
    add_files("loader/Main.def")
    
    -- precompiled header
    set_pcxxheader("loader/stdafx.hpp")

    -- links
    add_packages("wil", "fmt")

    -- Post-build: copy TWASE.dll and PDB to <gameroot>
    after_build(function (target)
        if has_config("disable_copy") then
            print("[twase] Skipping copy to game directory (disable_copy is enabled).")
            return
        end

        -- Resolve destination folder
        local destdir = path.join(gameroot)

        -- Only proceed if gameroot exists to avoid accidental folder creation in a wrong path
        if not os.isdir(gameroot) then
            print(string.format("[twase] Gameroot '%s' not found; skipping deploy copy.", gameroot))
            return
        end

        -- Ensure destination exists
        os.mkdir(destdir)

        -- Copy the built DLL
        local dll = target:targetfile()
        if dll and os.isfile(dll) then
            os.cp(dll, destdir)
            print(string.format("[twase] Copied DLL: %s -> %s", dll, destdir))
        else
            print("[twase] Warning: Built DLL not found; nothing to copy.")
        end
    end)

target("twase")
    set_basename("TWASE")
    set_kind("shared")

    -- files
    add_files("dll/*.cpp")
    add_headerfiles("dll/*.hpp")
    for _, dir in ipairs(os.dirs("dll/*")) do
        if os.isdir(dir) then
            add_files(path.join(dir, "*.cpp"))
            add_headerfiles(path.join(dir, "*.hpp"))
        end
    end
    add_files("dll/*.rc")

    -- sdk
    -- add_files("sdk/Attila/*.cpp")
    add_headerfiles("sdk/Attila/*.hpp")
    for _, dir in ipairs(os.dirs("sdk/Attila/*")) do
        if os.isdir(dir) then
            add_files(path.join(dir, "*.cpp"))
            add_headerfiles(path.join(dir, "*.hpp"))
        end
    end

    -- precompiled header
    set_pcxxheader("dll/stdafx.hpp")

    -- links
    add_packages("wil", "fmt", "spdlog", "toml11", "ordered_map", "microsoft-detours", "imgui", "lua")

    add_syslinks("d3d11", "dxgi", "dinput8", "ntdll")

    on_run(function (target)
        local gr = os.getenv("TWASE_GAMEROOT") or [[D:/SteamLibrary/steamapps/common/Total War Attila]]
        local exe = path.join(gr, "Attila.exe")
        if not os.isfile(exe) then
            raise("[twase] Attila.exe not found at '%s'. Set TWASE_GAMEROOT correctly.", exe)
        end
        os.execv(exe, {}, { curdir = gr })
    end)

    after_build(function (target)
        if has_config("disable_copy") then
            print("[twase] Skipping copy to game directory (disable_copy is enabled).")
            return
        end

        -- Resolve destination folder
        local destdir = path.join(gameroot, "TWASE")

        -- Only proceed if gameroot exists to avoid accidental folder creation in a wrong path
        if not os.isdir(gameroot) then
            print(string.format("[twase] Gameroot '%s' not found; skipping deploy copy.", gameroot))
            return
        end

        -- Ensure destination exists
        os.mkdir(destdir)

        -- Copy the built DLL
        local dll = target:targetfile()
        if dll and os.isfile(dll) then
            os.cp(dll, destdir)
            print(string.format("[twase] Copied DLL: %s -> %s", dll, destdir))
        else
            print("[twase] Warning: Built DLL not found; nothing to copy.")
        end

         -- Try to copy the PDB (if generated)
        local pdb
        if dll then
            local dllname = path.filename(dll)                  -- e.g. TWASE.dll
            local base = dllname and dllname:gsub("%.[^%.]+$", "") -- strip extension safely
            if base then
                pdb = path.join(target:targetdir(), base .. ".pdb")
            end
        end
        if pdb and os.isfile(pdb) then
            os.cp(pdb, destdir)
            print(string.format("[twase] Copied PDB: %s -> %s", pdb, destdir))
        else
            -- Silently skip if no PDB (e.g. release without symbols)
        end
    end)

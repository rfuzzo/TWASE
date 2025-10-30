set_project("twase")
set_arch("x86")

set_languages("c++23")
set_optimize("faster")
set_warnings("allextra")
set_encodings("utf-8")

add_rules("mode.debug", "mode.release")

-- Hardcoded game root for post-build deployment (can be overridden with TWASE_GAMEROOT env var)
-- Note: forward slashes are fine on Windows; adjust to your install if needed.
local gameroot = os.getenv("TWASE_GAMEROOT") or [[D:/SteamLibrary/steamapps/common/Total War Attila]]

add_requires("wil", "fmt", "spdlog", "toml11", "ordered_map")

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

-- add _DEBUG for debug builds
if is_mode("debug") then
    add_defines("_DEBUG")
end


add_syslinks("User32", "shell32", "ole32", "version")

target("loader")
    set_basename("winmm") 
    set_kind("shared")
    
    -- files
    add_files("src/loader/*.cpp")    
    add_headerfiles("src/loader/*.hpp")

    add_files("src/loader/Proxies/*.cpp")
    add_headerfiles("src/loader/Proxies/*.hpp")

    add_files("src/loader/*.rc")
    add_files("src/loader/Main.def")
    
    -- precompiled header
    set_pcxxheader("src/loader/stdafx.hpp")

    -- links
    add_packages("wil", "fmt")

    -- Post-build: copy TWASE.dll and PDB to <gameroot>
    after_build(function (target)
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
    

target("twase")
    set_basename("TWASE")
    set_kind("shared")

    add_files("src/dll/*.cpp")
    add_headerfiles("src/dll/*.hpp")

    add_files("src/dll/*.rc")

    -- precompiled header
    set_pcxxheader("src/dll/stdafx.hpp")

    -- links
    add_packages("wil", "fmt", "spdlog", "toml11", "ordered_map")

    after_build(function (target)
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
    end)


-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

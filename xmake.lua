set_project("twase")

set_languages("c++23")
set_optimize("faster")
set_warnings("allextra")
set_encodings("utf-8")

add_rules("mode.debug", "mode.release")

-- add _DEBUG for debug builds
if is_mode("debug") then
    add_defines("_DEBUG")
end

option("disable_copy", {default = false, description = "Disable copy build files to game directory after build."})

-- one TWASE per game binary: Windows (Attila.exe, x86) and macOS (Feral port, arm64)
if is_plat("windows") then
    set_arch("x86")
    includes("src/platform/win32")
elseif is_plat("macosx") then
    set_arch("arm64")
    includes("src/platform/macos")
end

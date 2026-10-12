set_project("twase_spike_inject")
set_arch("arm64")
set_languages("c++23")

add_rules("mode.debug", "mode.release")

target("spike")
    set_kind("shared")
    set_basename("twase_spike")
    add_files("spike.cpp")



target("core")
    set_kind("static")
    set_languages("c99", "c++20")
    add_files("**.cpp")
    add_includedirs(
        "$(projectdir)/external/bvh",
        "$(projectdir)/external/metis/include",
        "$(projectdir)/external/spdlog/include",
        { public=true }
    )
    add_linkdirs(
        "$(projectdir)/external/metis/lib",
        "$(projectdir)/external/spdlog/lib",
        { public=true }
    )
    add_links(
        "metis.lib",
        "spdlog.lib",
        { public=true }
    )
target_end()
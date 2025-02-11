
target("render_pass")
    set_kind("static")
    set_languages("c99", "c++20")
    add_files("**.cpp")

    add_deps("gui", { public=true })
    add_deps("render_graph", { public=true })

    add_includedirs(
        "$(projectdir)/external/cyCodeBase",
        { public=true }
    )
    add_linkdirs(
        { public=true }
    )
    add_links(
        "vulkan-1.lib",
        { public=true }
    )
target_end()
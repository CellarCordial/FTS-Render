
target("scene")
    set_kind("static")
    set_languages("c99", "c++20")
    add_files("**.cpp")

    add_deps("core", { public=true })
    add_deps("gui", { public=true })

    add_includedirs(
        "$(projectdir)/external/glfw-3.4.bin.WIN64/include",
        "$(projectdir)/external/stb",
        "$(projectdir)/external/assimp/include",
        "$(projectdir)/external/minizip/include",
        "$(projectdir)/external/zlib/include",
        "$(projectdir)/external/gklib/include",
        { public=true }
    )
    add_linkdirs(
        "$(projectdir)/external/assimp/lib",
        "$(projectdir)/external/zlib/lib",
        "$(projectdir)/external/gklib/lib",
        "$(projectdir)/external/minizip/lib",
        { public=true }
    )
    add_links(
        "assimp-vc143-mt.lib",
        "zlib.lib",
        "gklib.lib",
        "minizip.lib",
        { public=true }
    )
target_end()


target("render_graph")
    set_kind("static")
    set_languages("c99", "c++20")
    add_files("**.cpp")
    add_deps("dynamic_rhi", { public=true })
target_end()
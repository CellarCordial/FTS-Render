

target("gui")
    set_kind("static")
    set_languages("c99", "c++20")
    add_files("**.cpp")
    add_files(
		"$(projectdir)/external/imgui/*.cpp", 
        "$(projectdir)/external/imgui/backends/imgui_impl_dx12.cpp", 
        "$(projectdir)/external/imgui/backends/imgui_impl_glfw.cpp",
        "$(projectdir)/external/imgui/backends/imgui_impl_opengl3.cpp"
    )
    add_deps("dynamic_rhi", { public=true })
    add_includedirs(
        "$(projectdir)/external/imgui", 
        "$(projectdir)/external/glfw-3.4.bin.WIN64/include",
        { public=true }
    )
target_end()
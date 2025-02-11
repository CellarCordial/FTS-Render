
target("test")
    set_kind("binary")
    set_languages("c99", "c++20")
    add_files("**.cpp")

    add_deps("scene", { public=true })
    add_deps("shader", { public=true })
    add_deps("render_pass", { public=true })
    add_includedirs("$(projectdir)/external/glfw-3.4.bin.WIN64/include", { public=true })
    add_linkdirs("$(projectdir)/external/glfw-3.4.bin.WIN64/lib-static-ucrt", { public=true })
    add_links("glfw3dll.lib", { public=true })
    after_build(
        function (target)
            os.cp("$(projectdir)/external/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3.dll", target:targetdir())
        end
    )
target_end()
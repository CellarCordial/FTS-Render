

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
            os.cp("$(projectdir)/external/dxc/bin/dxcompiler.dll", target:targetdir())
            os.cp("$(projectdir)/external/dxc/bin/dxil.dll", target:targetdir())

            os.mkdir(path.join(target:targetdir(), "D3D12"))
            os.cp("$(projectdir)/external/D3D12SDK/bin/D3D12Core.dll", path.join(target:targetdir(), "D3D12"))
            os.cp("$(projectdir)/external/D3D12SDK/bin/D3D12SDKLayers.dll", path.join(target:targetdir(), "D3D12"))
        end
    )
target_end()
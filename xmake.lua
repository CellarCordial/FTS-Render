add_rules("mode.debug", "mode.release")

-- If set to ".vscode", the Clangd will not indexing all files even if you have set "compile-commands-dir".
add_rules("plugin.compile_commands.autoupdate", {outputdir = "$(projectdir)"})

set_runtimes("MD")
local ProjDir = os.projectdir()
local NormalizedProjDir = ProjDir:gsub("\\", "/")

target("FTS-Render")
    set_kind("binary")
    set_languages("c99", "c++20")
    add_defines(
        "NDEBUG", 
    	"DEBUG",
        "NOMINMAX",
        "D3D12_API",
        "SLANG_SHADER",
        "RAY_TRACING=1",
        "NUM_FRAMES_IN_FLIGHT=3",
        "CLIENT_WIDTH=1024",
        "CLIENT_HEIGHT=768",
        "PROJ_DIR=\"" .. NormalizedProjDir .. "/\""
    )
    add_files(
		"$(projectdir)/source/gui/**.cpp",
		"$(projectdir)/source/core/**.cpp",
		"$(projectdir)/source/test/**.cpp",
		"$(projectdir)/source/scene/**.cpp",
		"$(projectdir)/source/shader/*.cpp",
		"$(projectdir)/source/global_render.cpp",
		"$(projectdir)/source/dynamic_rhi/**.cpp",
        "$(projectdir)/source/render_pass/**.cpp",
		"$(projectdir)/source/render_graph/**.cpp",
		"$(projectdir)/source/main.cpp"
    )
    add_files(
		"$(projectdir)/external/imgui/*.cpp", 
        "$(projectdir)/external/imgui/backends/imgui_impl_dx12.cpp", 
        "$(projectdir)/external/imgui/backends/imgui_impl_glfw.cpp",
        "$(projectdir)/external/imgui/backends/imgui_impl_opengl3.cpp"
    ) 
    add_includedirs(
        "$(projectdir)/external/stb",
        "$(projectdir)/external/bvh",
        "$(projectdir)/external/imgui",
        "$(projectdir)/external/tinygltf",
        "$(projectdir)/external/cyCodeBase"
    )
    add_includedirs(
        "$(projectdir)/external/glfw-3.4.bin.WIN64/include/GLFW",
        "$(projectdir)/external/spdlog/include",
        "$(projectdir)/external/DirectXShaderCompiler/inc",
        "$(projectdir)/external/slang/include"
    )
    add_linkdirs(
        "$(projectdir)/external/glfw-3.4.bin.WIN64/lib-static-ucrt",
        "$(projectdir)/external/spdlog/build/Release",
        "$(projectdir)/external/DirectXShaderCompiler/lib/x64",
        "$(projectdir)/external/slang/lib"
    )
    add_links(
        "glfw3dll.lib",
        "spdlog.lib",
        "dxcompiler.lib",
        "slang.lib"
    )
    after_build(
        function (target)
            local DllFiles = {
                "$(projectdir)/external/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3.dll",
                "$(projectdir)/external/DirectXShaderCompiler/bin/x64/dxil.dll",
                "$(projectdir)/external/DirectXShaderCompiler/bin/x64/dxcompiler.dll"
            }

            for _, File in ipairs(DllFiles) do
                os.cp(File, target:targetdir())
            end
        end
    )


target("VulkanTest")
    set_kind("binary")
    set_languages("c99", "c++20")
    add_defines(
        "NDEBUG", 
    	"DEBUG",
        "NOMINMAX",
        "NUM_FRAMES_IN_FLIGHT=3",
        "CLIENT_WIDTH=1024",
        "CLIENT_HEIGHT=768",
        "PROJ_DIR=$(projectdir)/"
    )
    add_files(
		"$(projectdir)/vulkan_test/*.cpp"
    )
    add_includedirs(
        "$(projectdir)/external/stb"
    )
    add_includedirs(
        "$(projectdir)/external/glfw-3.4.bin.WIN64/include/GLFW",
        "$(projectdir)/external/spdlog/include",
        "$(projectdir)/external/VulkanSDK/Include"
    )
    add_linkdirs(
        "$(projectdir)/external/glfw-3.4.bin.WIN64/lib-static-ucrt",
        "$(projectdir)/external/spdlog/build/Release",
        "$(projectdir)/external/VulkanSDK/Lib"
    )
    add_links(
        "glfw3dll.lib",
        "spdlog.lib",
        "vulkan-1.lib"
    )
    after_build(
        function (target)
            local DllFiles = {
                "$(projectdir)/external/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3.dll"
            }

            for _, File in ipairs(DllFiles) do
                os.cp(File, "$(buildir)/inc")
            end
        end
    )


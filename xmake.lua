add_rules("mode.debug", "mode.release")

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
        "USE_RAY_TRACING=0",
        "NUM_FRAMES_IN_FLIGHT=3",
        "CLIENT_WIDTH=1024",
        "CLIENT_HEIGHT=768",
        "PROJ_DIR=\"" .. NormalizedProjDir .. "/\""
    )
    add_files(
		"$(projectdir)/Source/Shader/*.cpp",
		"$(projectdir)/Source/Gui/**.cpp",
		"$(projectdir)/Source/Core/**.cpp",
		"$(projectdir)/Source/Math/**.cpp",
		"$(projectdir)/Source/Scene/**.cpp",
		"$(projectdir)/Source/Parallel/**.cpp",
		"$(projectdir)/Source/DynamicRHI/**.cpp",
		"$(projectdir)/Source/RenderGraph/**.cpp",
        "$(projectdir)/Source/RenderPass/**.cpp",
		"$(projectdir)/Source/GlobalRender.cpp",
		"$(projectdir)/Source/main.cpp"
    )
    add_files(
		"$(projectdir)/External/imgui/*.cpp", 
        "$(projectdir)/External/imgui/backends/imgui_impl_dx12.cpp", 
        "$(projectdir)/External/imgui/backends/imgui_impl_glfw.cpp",
        "$(projectdir)/External/imgui/backends/imgui_impl_opengl3.cpp"
    ) 
    add_includedirs(
        "$(projectdir)/External/stb",
        "$(projectdir)/External/imgui",
        "$(projectdir)/External/tinygltf",
        "$(projectdir)/External/cyCodeBase",
        "$(projectdir)/External/bvh"
    )
    add_includedirs(
        "$(projectdir)/External/glfw-3.4.bin.WIN64/include/GLFW",
        "$(projectdir)/External/spdlog/include",
        "$(projectdir)/External/DirectXShaderCompiler/inc",
        "$(projectdir)/External/meshoptimizer/src"
    )
    add_linkdirs(
        "$(projectdir)/External/glfw-3.4.bin.WIN64/lib-static-ucrt",
        "$(projectdir)/External/spdlog/build/Release",
        "$(projectdir)/External/DirectXShaderCompiler/lib/x64",
        "$(projectdir)/External/meshoptimizer/build/Debug"
    )
    add_links(
        "glfw3dll.lib",
        "spdlog.lib",
        "dxcompiler.lib",
        "meshoptimizer.lib"
    )
    after_build(
        function (target)
            local DllFiles = {
                "$(projectdir)/External/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3.dll",
                "$(projectdir)/External/DirectXShaderCompiler/bin/x64/dxil.dll",
                "$(projectdir)/External/DirectXShaderCompiler/bin/x64/dxcompiler.dll"
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
		"$(projectdir)/VulkanTest/*.cpp"
    )
    add_includedirs(
        "$(projectdir)/External/stb"
    )
    add_includedirs(
        "$(projectdir)/External/glfw-3.4.bin.WIN64/include/GLFW",
        "$(projectdir)/External/spdlog/include",
        "$(projectdir)/External/VulkanSDK/Include"
    )
    add_linkdirs(
        "$(projectdir)/External/glfw-3.4.bin.WIN64/lib-static-ucrt",
        "$(projectdir)/External/spdlog/build/Release",
        "$(projectdir)/External/VulkanSDK/Lib"
    )
    add_links(
        "glfw3dll.lib",
        "spdlog.lib",
        "vulkan-1.lib"
    )
    after_build(
        function (target)
            local DllFiles = {
                "$(projectdir)/External/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3.dll"
            }

            for _, File in ipairs(DllFiles) do
                os.cp(File, "$(buildir)/inc")
            end
        end
    )


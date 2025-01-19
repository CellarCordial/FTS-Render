add_rules("mode.debug", "mode.release")

-- If set to ".vscode", the Clangd will not indexing all files even if you have set "compile-commands-dir".
add_rules("plugin.compile_commands.autoupdate", {outputdir = "$(projectdir)"})

set_runtimes("MD")
local proj_dir = os.projectdir()
local normalized_proj_dir = proj_dir:gsub("\\", "/")

target("FTS-Render")
    set_kind("binary")
    set_languages("c99", "c++20")
    add_defines(
        "NDEBUG", 
    	"DEBUG",
        "NOMINMAX",
        "D3D12_API",
        "NUM_FRAMES_IN_FLIGHT=3",
        "CLIENT_WIDTH=1024",
        "CLIENT_HEIGHT=768",
        "PROJ_DIR=\"" .. normalized_proj_dir .. "/\""
    )
    add_files("$(projectdir)/source/**.cpp")
    add_files(
		"$(projectdir)/external/imgui/*.cpp", 
        "$(projectdir)/external/imgui/backends/imgui_impl_dx12.cpp", 
        "$(projectdir)/external/imgui/backends/imgui_impl_glfw.cpp",
        "$(projectdir)/external/imgui/backends/imgui_impl_opengl3.cpp"
    ) 
    add_includedirs(
        "$(projectdir)/external/bvh",
        "$(projectdir)/external/stb",
        "$(projectdir)/external/bvh",
        "$(projectdir)/external/imgui",
        "$(projectdir)/external/cyCodeBase",
        "$(projectdir)/external/zlib/include",
        "$(projectdir)/external/slang/include",
        "$(projectdir)/external/gklib/include",
        "$(projectdir)/external/metis/include",
        "$(projectdir)/external/assimp/include",
        "$(projectdir)/external/spdlog/include",
        "$(projectdir)/external/minizip/include",
        "$(projectdir)/external/VulkanSDK/include",
        "$(projectdir)/external/glfw-3.4.bin.WIN64/include"
    )
    add_linkdirs(
        "$(projectdir)/external/zlib/lib",
        "$(projectdir)/external/slang/lib",
        "$(projectdir)/external/gklib/lib",
        "$(projectdir)/external/metis/lib",
        "$(projectdir)/external/spdlog/lib",
        "$(projectdir)/external/assimp/lib",
        "$(projectdir)/external/minizip/lib",
        "$(projectdir)/external/VulkanSDK/lib",
        "$(projectdir)/external/glfw-3.4.bin.WIN64/lib-static-ucrt"
    )
    add_links(
        "zlib.lib",
        "slang.lib",
        "gklib.lib",
        "metis.lib",
        "spdlog.lib",
        "minizip.lib",
        "glfw3dll.lib",
        "vulkan-1.lib",
        "assimp-vc143-mt.lib"
    )

    after_build(
        function (target)
            local DllFiles = {
                "$(projectdir)/external/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3.dll",
                "$(projectdir)/external/slang/bin/slang.dll"
            }

            for _, File in ipairs(DllFiles) do
                os.cp(File, target:targetdir())
            end
        end
    )
target_end()


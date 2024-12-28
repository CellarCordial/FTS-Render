add_rules("mode.debug", "mode.release")

-- If set to ".vscode", the Clangd will not indexing all files even if you have set "compile-commands-dir".
add_rules("plugin.compile_commands.autoupdate", {outputdir = "$(projectdir)"})

set_runtimes("MD")
local ProjDir = os.projectdir()
local NormalizedProjDir = ProjDir:gsub("\\", "/")

add_requires("metis", "assimp", "spdlog", "slang", "glfw", "stb", "vulkansdk");
add_requires("imgui", {configs = {dx12 = true, glfw_opengl3 = true}})

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
    add_includedirs(
        "$(projectdir)/external/bvh",
        "$(projectdir)/external/cyCodeBase",
        "$(projectdir)/external/imgui_helper"
    )
    add_packages("metis", "assimp", "spdlog", "slang", "glfw", "stb", "imgui")

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
    add_packages("spdlog", "glfw", "vulkansdk")

add_rules("mode.debug", "mode.release")

-- If set to ".vscode", the Clangd will not indexing all files even if you have set "compile-commands-dir".
add_rules("plugin.compile_commands.autoupdate", {outputdir = "$(projectdir)"})
set_runtimes("MD")
set_languages("c++20")

local proj_dir = os.projectdir()
local normalized_proj_dir = proj_dir:gsub("\\", "/")

add_defines(
    "DEBUG", 
    "NOMINMAX", 
    "HLSL_SHADER",
    "CLIENT_WIDTH=1024",
    "CLIENT_HEIGHT=768",
    "FLIGHT_FRAME_NUM=3", 
    "PROJ_DIR=\"" .. normalized_proj_dir .. "/\""
)

includes("source/core")
includes("source/dynamic_rhi")
includes("source/render_graph")
includes("source/gui")
includes("source/render_pass")
includes("source/scene")
includes("source/shader")
includes("source/test")

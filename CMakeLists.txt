cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)

project(FTSRender)
	set(Target0 FTSRender)

    file(
        GLOB_RECURSE 
        CppSource
		"${PROJECT_SOURCE_DIR}/Source/Gui/*.cpp"
		"${PROJECT_SOURCE_DIR}/Source/Core/*.cpp"
		"${PROJECT_SOURCE_DIR}/Source/Math/*.cpp"
		"${PROJECT_SOURCE_DIR}/Source/Scene/*.cpp"
		"${PROJECT_SOURCE_DIR}/Source/Shader/*.cpp"
		"${PROJECT_SOURCE_DIR}/Source/TaskFlow/*.cpp"
		"${PROJECT_SOURCE_DIR}/Source/DynamicRHI/*.cpp"
		"${PROJECT_SOURCE_DIR}/Source/RenderGraph/*.cpp"
        	"${PROJECT_SOURCE_DIR}/Source/RenderPass/*.cpp"
		"${PROJECT_SOURCE_DIR}/Source/GlobalRender.cpp"
		"${PROJECT_SOURCE_DIR}/Source/main.cpp"
    )

    file(
        GLOB
        ImguiSource
        "${PROJECT_SOURCE_DIR}/External/imgui/*.cpp", 
        "${PROJECT_SOURCE_DIR}/External/imgui/backends/imgui_impl_dx12.cpp", 
        "${PROJECT_SOURCE_DIR}/External/imgui/backends/imgui_impl_glfw.cpp",
        "${PROJECT_SOURCE_DIR}/External/imgui/backends/imgui_impl_opengl3.cpp"
    )

	add_executable(${Target0} ${CppSource} ${ImguiSource})

	target_compile_definitions(
		${Target0}
		PUBLIC
		DEBUG
        	NOMINMAX
        	D3D12_API
        	NUM_FRAMES_IN_FLIGHT=3
        	CLIENT_WIDTH=1024
        	CLIENT_HEIGHT=768
        	PROJ_DIR="${PROJECT_SOURCE_DIR}/"
	)

    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/stb")
    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/imgui")
    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/tinygltf")
    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/cyCodeBase")
    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/bvh")
	
    
    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/glfw-3.4.bin.WIN64/include/GLFW")
    target_link_libraries(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3dll.lib")

    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/spdlog/include")
    target_link_libraries(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/spdlog/build/Release/spdlog.lib")

    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/DirectXShaderCompiler/inc")
    target_link_libraries(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/DirectXShaderCompiler/lib/x64/dxcompiler.lib")
    
    target_include_directories(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/meshoptimizer/src")
    target_link_libraries(${Target0} PUBLIC "${PROJECT_SOURCE_DIR}/External/meshoptimizer/build/Debug/meshoptimizer.lib")

    add_custom_command(
        TARGET ${Target0} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${PROJECT_SOURCE_DIR}/External/glfw-3.4.bin.WIN64/lib-static-ucrt/glfw3.dll"
            $<TARGET_FILE_DIR:${Target0}>
        COMMAND ${CMAKE_COMMAND} -E copy
            "${PROJECT_SOURCE_DIR}/External/DirectXShaderCompiler/bin/x64/dxil.dll"
            $<TARGET_FILE_DIR:${Target0}>
    )
    

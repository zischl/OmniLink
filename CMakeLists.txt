cmake_minimum_required(VERSION 3.18)
project(OmniLink LANGUAGES CXX CUDA)


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
cmake_policy(SET CMP0104 NEW)
find_package(CUDAToolkit REQUIRED)
set(CMAKE_CUDA_ARCHITECTURES 75 80 86 89 90 100 120)



add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/debug/VertexShader.cso
    COMMAND fxc /T vs_5_0 /E VSMain /Fo debug/VertexShader.cso /Fc vsshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/vertex_shader.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/vertex_shader.hlsl
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/debug/PixelShader.cso
    COMMAND fxc /T ps_5_0 /E PSMain /Fo debug/PixelShader.cso /Fc psshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/pixel_shader.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/pixel_shader.hlsl
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/debug/BGRA2NV12Shader.cso
    COMMAND fxc /T cs_5_0 /E CSMain /Fo debug/BGRA2NV12Shader.cso /Fc csshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/BGRA2NV12Shader.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/BGRA2NV12Shader.hlsl
)

add_custom_target(CompileShaders ALL
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/debug/VertexShader.cso
        ${CMAKE_CURRENT_BINARY_DIR}/debug/PixelShader.cso
        ${CMAKE_CURRENT_BINARY_DIR}/debug/BGRA2NV12Shader.cso
)




include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/nvidia)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/lz4)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/cuda)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui/backends)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)


if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(architecture x64)
else()
    set(architecture x86)
    #set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/x86/)
endif()


link_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/lib/$<CONFIG>/${architecture}/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Universal/${architecture}/)



add_executable(main WIN32 
    src/main.cpp
    src/RendererCore.cpp
    src/SessionHandler.cpp
    src/nvenc.cpp
    src/nvdec.cpp
    src/CuKernel.cu
    src/WinForge.cpp
    src/WinCap.cpp
    src/OmniRenderer.cpp
    external/include/imgui/imgui.cpp
    external/include//imgui/imgui_draw.cpp
    external/include/imgui/imgui_tables.cpp
    external/include/imgui/imgui_widgets.cpp
    external/include/imgui/imgui_demo.cpp
    external/include/imgui/backends/imgui_impl_win32.cpp
    external/include/imgui/backends/imgui_impl_dx11.cpp
    src/OmniLink.cpp
    src/IOLink.cpp
    src/OmniLogger.cpp
    src/OmniGUI.cpp
    src/OmniDiscovery.cpp
    src/OmniAPI.cpp
    
 )

target_compile_definitions(main PUBLIC 
    $<$<CONFIG:Debug>:OMNI_BUILD_DEBUG>
    $<$<CONFIG:Release>:OMNI_BUILD_RELEASE>
    
)

set(DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/dll/$<CONFIG>/${architecture}")

add_custom_command(
    TARGET main 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DLL_PATH}"
        $<TARGET_FILE_DIR:main>
)

set_target_properties(main PROPERTIES CUDA_SEPARABLE_COMPILATION ON CUDA_STANDARD 14)
add_dependencies(main CompileShaders)



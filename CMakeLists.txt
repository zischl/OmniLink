cmake_minimum_required(VERSION 3.18)
project(OmniLink LANGUAGES CXX CUDA)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
cmake_policy(SET CMP0104 NEW)
set(CMAKE_CUDA_ARCHITECTURES 50 52 60 61 70 75 80 86 89 90 100 120)



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


#vcpkg
set(-DCMAKE_PREFIX_PATH="G:/git/vcpkg/vcpkg/installed/x64-windows")
include_directories(G:/git/vcpkg/vcpkg/installed/x64-windows)


set(NVENC_SDK "${CMAKE_CURRENT_SOURCE_DIR}/Nvenc")

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/x64)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/cuda)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/imgui/backends)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(main WIN32 
    "src/main.cpp"
    "src/RendererCore.cpp"
    src/SessionHandler.cpp
    src/nvenc.cpp
    src/nvdec.cpp
    src/CuKernel.cu
    src/WinForge.cpp
    "src/WinCap.cpp"
    "src/OmniRenderer.cpp"
    "include/imgui/imgui.cpp"
    "include//imgui/imgui_draw.cpp"
    "include/imgui/imgui_tables.cpp"
    "include/imgui/imgui_widgets.cpp"
    "include/imgui/imgui_demo.cpp"
    "include/imgui/backends/imgui_impl_win32.cpp"
    "include/imgui/backends/imgui_impl_dx11.cpp"
    "src/OmniLink.cpp"
    "src/IOLink.cpp"
    "src/OmniLogger.cpp"
    "OmniGUI.cpp" "src/OmniGUI.cpp")

set_target_properties(main PROPERTIES CUDA_SEPARABLE_COMPILATION ON CUDA_STANDARD 14)
add_dependencies(main CompileShaders)


#lz4 shit
find_package(lz4 CONFIG REQUIRED)
target_link_libraries(main PRIVATE lz4::lz4)

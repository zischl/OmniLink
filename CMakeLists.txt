cmake_minimum_required(VERSION 3.15)
project(OmniLink)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
set(CMAKE_CXX_STANDARD 17)

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

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(dxgi WIN32 src/dxgi.cpp "src/renderer.cpp" src/SessionHandler.cpp "src/nvenc.cpp")

#add_executable(main src/main.cpp src/SessionHandler.cpp)
#target_compile_definitions(main PRIVATE _WIN32_WINNT=0x0601)

add_dependencies(dxgi CompileShaders)


#lz4 shit
find_package(lz4 CONFIG REQUIRED)
target_link_libraries(dxgi PRIVATE lz4::lz4 ${NVENC_SDK}/Lib/x64/nvencodeapi.lib)

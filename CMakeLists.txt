cmake_minimum_required(VERSION 3.18)
project(OmniLink LANGUAGES CXX CUDA)


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
cmake_policy(SET CMP0104 NEW)
find_package(CUDAToolkit REQUIRED)
set(CMAKE_CUDA_ARCHITECTURES 75 80 86 89 90 100 120)



add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/VertexShader.cso
    COMMAND fxc /T vs_5_0 /E VSMain /Fo VertexShader.cso /Fc vsshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows/shaders/vertex_shader.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows/shaders/vertex_shader.hlsl
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/PixelShader.cso
    COMMAND fxc /T ps_5_0 /E PSMain /Fo PixelShader.cso /Fc psshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows/shaders/pixel_shader.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows/shaders/pixel_shader.hlsl
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/BGRA2NV12Shader.cso
    COMMAND fxc /T cs_5_0 /E CSMain /Fo BGRA2NV12Shader.cso /Fc csshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows/shaders/BGRA2NV12Shader.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows/shaders/BGRA2NV12Shader.hlsl
)

add_custom_target(CompileShaders ALL
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/VertexShader.cso
        ${CMAKE_CURRENT_BINARY_DIR}/PixelShader.cso
        ${CMAKE_CURRENT_BINARY_DIR}/BGRA2NV12Shader.cso
)




include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/nvidia)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/lz4)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/cuda)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui/backends)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/core)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/net)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/capture)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/codec)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/render)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/input)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/gui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/app)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/net/platform/windows)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/capture/platform/windows)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/input/platform/windows)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/app/platform/windows)


include_directories(${CMAKE_CURRENT_SOURCE_DIR})




if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(architecture x64)
else()
    set(architecture x86)
    #set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/x86/)
endif()


link_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/lib/$<CONFIG>/${architecture}/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Universal/${architecture}/)

if(WIN32)
    set(SourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows)
else()
    set(SourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/unix)
endif()

add_executable(main WIN32 
    src/app/platform/windows/main.cpp
    src/render/platform/windows/RendererCore.cpp
    src/net/platform/windows/SessionHandler.cpp
    src/codec/nvenc.cpp
    src/codec/nvdec.cpp
    src/codec/CuKernel.cu
    src/render/WinForge.cpp
    src/capture/platform/windows/WinCap.cpp
    src/render/platform/windows/OmniRenderer.cpp
    external/include/imgui/imgui.cpp
    external/include//imgui/imgui_draw.cpp
    external/include/imgui/imgui_tables.cpp
    external/include/imgui/imgui_widgets.cpp
    external/include/imgui/imgui_demo.cpp
    external/include/imgui/backends/imgui_impl_win32.cpp
    external/include/imgui/backends/imgui_impl_dx11.cpp
    src/app/OmniLink.cpp
    src/input/platform/windows/IOLink.cpp
    src/core/OmniLogger.cpp
    src/gui/OmniGUI.cpp
    src/net/OmniDiscovery.cpp
    src/app/OmniAPI.cpp
    src/misc/platform/windows/system_probe.cpp
    src/capture/platform/windows/AudioCap.cpp
    src/core/ByteStream.cpp
    resource.rc

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


# I don't wanna manually copy compile commands to root each time I generate for nvim. The End.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    if(CMAKE_HOST_WIN32)
file(WRITE "${CMAKE_SOURCE_DIR}/.clangd" 
"CompileFlags:
  CompilationDatabase: \"${CMAKE_BINARY_DIR}\"
"
    )
    else()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/compile_commands.json")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
        )
    endif()
    endif()
endif()


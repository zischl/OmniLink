cmake_minimum_required(VERSION 3.18)
project(OmniLink LANGUAGES CXX CUDA)

if(MSVC)
  add_link_options(/SUBSYSTEM:WINDOWS)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
cmake_policy(SET CMP0104 NEW)

# Gimme CUDA
find_package(CUDAToolkit REQUIRED)
set(CMAKE_CUDA_ARCHITECTURES
    75
    80
    86
    89
    90
    100
    120)

# Automatic Shader GLOB
set(SHADER_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows/shaders")
file(GLOB HLSL_SHADERS "${SHADER_DIR}/*.hlsl")

foreach(SHADER_PATH ${HLSL_SHADERS})
  get_filename_component(SHADER_NAME ${SHADER_PATH} NAME_WE)

  set(PROFILE "cs_5_0")
  if(${SHADER_NAME} MATCHES "[Vv]ertex" OR ${SHADER_NAME} MATCHES "_vs")
    set(PROFILE "vs_5_0")
  elseif(${SHADER_NAME} MATCHES "[Pp]ixel" OR ${SHADER_NAME} MATCHES "_ps")
    set(PROFILE "ps_5_0")
  endif()

  set(OUTPUT_CSO "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.cso")
  list(APPEND SHADER_OUTPUTS ${OUTPUT_CSO})

  add_custom_command(
    OUTPUT ${OUTPUT_CSO}
    COMMAND
      fxc /T ${PROFILE} /E CSMain /Fo ${OUTPUT_CSO} /Fc
      ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}Assembly.txt ${SHADER_PATH}
    DEPENDS ${SHADER_PATH}
    VERBATIM)
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SHADER_OUTPUTS})

# ImGui and resource file GLOBS
file(GLOB IMGUI_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui/backends/*.cpp")
set(RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resource.rc")

# OmniLink C++ and CUDA source files GLOBS
file(GLOB_RECURSE PROJECT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cu")

# Targets.....
add_executable(main WIN32 ${IMGUI_SOURCES} ${RESOURCE_FILES} ${PROJECT_SOURCES})

set_target_properties(
  main
  PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE
             CUDA_SEPARABLE_COMPILATION ON
             CUDA_STANDARD 14)

add_dependencies(main CompileShaders)

# Surely this is self explanatory
target_include_directories(
  main
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/nvidia
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/lz4
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/cuda
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui/backends
          ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/src/core
          ${CMAKE_CURRENT_SOURCE_DIR}/src/net
          ${CMAKE_CURRENT_SOURCE_DIR}/src/capture
          ${CMAKE_CURRENT_SOURCE_DIR}/src/codec
          ${CMAKE_CURRENT_SOURCE_DIR}/src/render
          ${CMAKE_CURRENT_SOURCE_DIR}/src/input
          ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
          ${CMAKE_CURRENT_SOURCE_DIR}/src/app
          ${CMAKE_CURRENT_SOURCE_DIR}/src/misc
          ${CMAKE_CURRENT_SOURCE_DIR}/src/net/platform/windows
          ${CMAKE_CURRENT_SOURCE_DIR}/src/capture/platform/windows
          ${CMAKE_CURRENT_SOURCE_DIR}/src/render/platform/windows
          ${CMAKE_CURRENT_SOURCE_DIR}/src/input/platform/windows
          ${CMAKE_CURRENT_SOURCE_DIR}/src/app/platform/windows
          ${CMAKE_CURRENT_SOURCE_DIR}/src/window/platform/windows
          ${CMAKE_CURRENT_SOURCE_DIR}/src/misc/platform/windows)

# Yup... Still about Targets
target_compile_definitions(main PUBLIC $<$<CONFIG:Debug>:OMNI_BUILD_DEBUG>
                                       $<$<CONFIG:Release>:OMNI_BUILD_RELEASE>)

# Build architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(architecture x64)
else()
  set(architecture x86)
  # set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/x86/)
endif()

link_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/$<CONFIG>/${architecture}/)
link_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Universal/${architecture}/)

# Post Build Shit... Copying DLLs
set(DLL_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/external/dll/$<CONFIG>/${architecture}")

add_custom_command(
  TARGET main
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${DLL_PATH}"
          $<TARGET_FILE_DIR:main>)

# I don't wanna manually copy compile commands to root each time I generate for
# nvim. The End.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  if(CMAKE_HOST_WIN32)
    file(
      WRITE "${CMAKE_SOURCE_DIR}/.clangd"
      "CompileFlags:
  CompilationDatabase: \"${CMAKE_BINARY_DIR}\"
")
  else()
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/compile_commands.json")
      execute_process(
        COMMAND
          ${CMAKE_COMMAND} -E create_symlink
          "${CMAKE_BINARY_DIR}/compile_commands.json"
          "${CMAKE_SOURCE_DIR}/compile_commands.json")
    endif()
  endif()
endif()

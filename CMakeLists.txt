cmake_minimum_required(VERSION 3.15)
project(OmniLink)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


add_executable(main  "src/main.cpp" "src/main.cpp")


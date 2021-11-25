cmake_minimum_required(VERSION 3.19)
project(rune)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE RUNE_SRCS CONFIGURE_DEPENDS src/*.cpp)

add_executable(rune ${RUNE_SRCS})
target_include_directories(rune PRIVATE src/)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)
target_link_libraries(rune glfw)

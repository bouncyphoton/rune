cmake_minimum_required(VERSION 3.19)
project(rune)

set(CMAKE_CXX_STANDARD 17)

#file(GLOB_RECURSE RUNE_SRCS CONFIGURE_DEPENDS src/*.cpp src/*.c)
#add_executable(rune ${RUNE_SRCS})
add_executable(rune src/config.cpp src/core.cpp src/gfx/graphics_backend.cpp src/main.cpp src/platform.cpp src/renderer.cpp src/gfx/render_pass.cpp src/gfx/graphics_pass.cpp external/SPIRV-Reflect/spirv_reflect.c)
target_include_directories(rune PRIVATE src/ external/SPIRV-Reflect)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)
target_link_libraries(rune glfw)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(rune Vulkan::Vulkan)

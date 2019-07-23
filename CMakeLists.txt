cmake_minimum_required(VERSION 3.14)
project(VkTriangle)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3 IMPORTED_TARGET)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_executable(VkTriangle)
target_sources(VkTriangle PRIVATE main.cpp)
target_link_libraries(VkTriangle PRIVATE Vulkan::Vulkan PkgConfig::GLFW glm)
if (APPLE)
    target_link_libraries(VkTriangle
        PRIVATE
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework IOKit"
            "-framework CoreVideo"
    )
endif()
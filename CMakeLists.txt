cmake_minimum_required(VERSION 3.14)
project(VkTriangle)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3 IMPORTED_TARGET)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(GLSLValidator REQUIRED)

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

add_custom_command(
    OUTPUT vert.spv
    COMMAND ${GLSL_VALIDATOR_EXECUTABLE} -V ${CMAKE_CURRENT_SOURCE_DIR}/shader.vert
)

add_custom_command(
    OUTPUT frag.spv
    COMMAND ${GLSL_VALIDATOR_EXECUTABLE} -V ${CMAKE_CURRENT_SOURCE_DIR}/shader.frag
)

add_custom_target(ShaderCompilation ALL
    DEPENDS vert.spv frag.spv
)
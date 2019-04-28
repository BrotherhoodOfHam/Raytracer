cmake_minimum_required(VERSION 3.7)

project(Raytracer)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

add_executable(
    Raytracer
    src/Main.cpp
)

target_link_libraries(Raytracer
    PRIVATE
      Vulkan::Vulkan
      SDL2::SDL2main
)

add_custom_command(
    TARGET Raytracer
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
     "${SDL2_RUNTIME_LIBRARY}" "$<TARGET_FILE_DIR:Raytracer>/SDL2.dll"
)
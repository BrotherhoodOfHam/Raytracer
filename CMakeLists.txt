cmake_minimum_required(VERSION 3.7)

project(Raytracer)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${PROJECT_SOURCE_DIR}/glm")

#######################################################################################
# Main program
#######################################################################################

set(_shaders
	shaders/a.vert
	shaders/a.frag
	shaders/quad.vert
	shaders/trace_sphere.frag
)

set(_srcs
    src/Main.cpp
	src/Utils.h
	src/Utils.cpp
	src/Common.h
	src/DebugExt.cpp
	src/DebugExt.h
    src/App.h
    src/App.cpp
	src/Triangle.h
	src/Triangle.cpp
	src/Camera.h
	src/Raytracer.h
	src/Raytracer.cpp
)

add_executable(
    Raytracer
	${_srcs}
	${_shaders}
)

source_group(TREE "${PROJECT_SOURCE_DIR}" FILES ${_srcs} ${_shaders})

target_link_libraries(Raytracer
    PRIVATE
      Vulkan::Vulkan
	  SDL2::SDL2main
	  glm
)


add_custom_command(
    TARGET Raytracer
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
     "${SDL2_RUNTIME_LIBRARY}" "$<TARGET_FILE_DIR:Raytracer>/SDL2.dll"
)

#######################################################################################
# Shader compilation
#######################################################################################

set(_shader_binaries)

foreach (_shader ${_shaders})
	get_filename_component(_shader_name ${_shader} NAME)
	set(_src   "${PROJECT_SOURCE_DIR}/${_shader}")
	set(_spirv "${PROJECT_BINARY_DIR}/shaders/${_shader_name}.spv")
	add_custom_command(
		OUTPUT ${_spirv}
		COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
		COMMAND ${GLSL_VALIDATOR} -V ${_src} -o ${_spirv}
		DEPENDS ${_src}
	)
	list(APPEND _shader_binaries ${_spirv})
endforeach()

add_custom_target(
	BUILD_SHADERS
	DEPENDS ${_shader_binaries}
)

add_dependencies(Raytracer BUILD_SHADERS)

#######################################################################################

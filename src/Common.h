/*
	Common defines
*/

#pragma once

//#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

// Log 1 or more values
template<typename A, typename ... T>
inline void log(A&& first, T&& ... rest)
{
	std::cout << first << " ";
	log(rest...);
}

template<typename ... Types>
inline void log()
{
	std::cout << std::endl;
}

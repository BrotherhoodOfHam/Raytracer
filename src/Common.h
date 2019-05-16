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

namespace Log
{
	template<typename A, typename ... T>
	inline void info(A&& first, T&& ... rest)
	{
		std::cout << first << " ";
		Log::info(rest...);
	}

	inline void info()
	{
		std::cout << std::endl;
	}

	template<typename ... T>
	inline void warn(T&& ... args)
	{
		Log::info("WARN:", args...);
	}

	template<typename ... T>
	inline void error(T&& ... args)
	{
		Log::info("ERROR:", args...);
	}
};

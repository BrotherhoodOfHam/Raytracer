/*
	A simple vulkan application

	Sets up a window with a vulkan instance/device
*/

#pragma once

#include <iostream>
#include <array>

//#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SDL_Window;

class App
{
private:

	//SDL2
	SDL_Window* _window;

	//Vulkan
	vk::SurfaceKHR         _surface;
	vk::SwapchainKHR       _swapchain;
	vk::Instance           _instance;
	vk::PhysicalDevice     _pdevice;
	vk::Device             _device;
	vk::Queue              _queue;
	std::vector<vk::Image> _swapchainImages;
	std::vector<vk::ImageView> _swapchainViews;

	uint32_t _queueFamilyIndex = 0;

	enum { FRAME_COUNT = 4 };

	struct Frame
	{
		vk::CommandBuffer commandBuffer;
		vk::Fence         fence;
		vk::Semaphore     imageAvailable;
		vk::Semaphore     renderFinished;
	};

	uint32_t        _frameIndex = 0;
	Frame           _frames[FRAME_COUNT];
	vk::CommandPool _commandPool;

public:

	App();
	~App();

	// Start application and block until it is stopped
	void run();

	// Get vulkan device
	const vk::Device& device() const { return _device; }

	// Log 1 or more values
	template<typename A, typename ... T>
	void log(A&& first, T&& ... rest)
	{
		std::cout << first << " ";
		log(rest...);
	}

	template<typename ... Types>
	void log()
	{
		std::cout << std::endl;
	}

private:

	void initSDL();
	
	void initVulkan();

	void init();

	void draw();

	void cleanup();

	//Utils
	std::exception error(const char* msg) { return std::exception(msg); }
};

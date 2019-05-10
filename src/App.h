/*
	A simple vulkan application

	Sets up a window with a vulkan instance/device
*/

#pragma once

#include <iostream>
#include <vector>

//#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

struct SDL_Window;

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
	uint32_t			   _queueFamilyIndex = 0;
	std::vector<vk::Image> _swapchainImages;
	vk::Format             _swapchainFormat;

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

	enum WindowDimensions
	{
		WIN_WIDTH = 800,
		WIN_HEIGHT = 800
	};

public:

	App();
	virtual ~App();

	App(const App&) = delete;

	// Start application and block until it is stopped
	void run();

	// Get device
	const vk::Device& device() const { return _device; }
	// Get swapchain images
	const std::vector<vk::Image>& swapchainImages() const { return _swapchainImages; }
	// Get swapchain format
	vk::Format swapchainFormat() const { return _swapchainFormat; }
	// Get swapchain width/height
	vk::Extent2D swapchainSize() const { return vk::Extent2D(WIN_WIDTH, WIN_HEIGHT); }

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

protected:

	//Events
	virtual void init() = 0;
	virtual void render(const vk::CommandBuffer& cmd, uint32_t swpIndex) = 0;
	virtual void destroy() = 0;

	using Buffer = std::vector<unsigned char>;

	//Utils
	std::exception error(const char* msg) { return std::exception(msg); }
	bool readContent(const char* path, Buffer& out);
	vk::ShaderModule loadModule(const char* path);

private:

	void initSDL();
	void initVulkan();

	void baseInit();
	void baseDestroy();
	void nextFrame();
};

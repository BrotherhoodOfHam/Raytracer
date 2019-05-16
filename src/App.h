/*
	A simple vulkan application

	Sets up a window with a vulkan instance/device
*/

#pragma once

#include <iostream>
#include <vector>
#include <chrono>

#include <SDL_events.h>

#include "Common.h"

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

	//debug
	vk::DebugReportCallbackEXT _debugReporter;
	vk::DebugUtilsMessengerEXT _debugMessenger;

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
		WIN_WIDTH = 1280,
		WIN_HEIGHT = 720
	};

	uint64_t _fps;
	std::chrono::high_resolution_clock::time_point _lastUpdate;

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
	// Get swapchain image count
	uint32_t swapchainCount() const { return (uint32_t)_swapchainImages.size(); }
	// Get swapchain format
	vk::Format swapchainFormat() const { return _swapchainFormat; }
	// Get swapchain width/height
	vk::Extent2D swapchainSize() const { return vk::Extent2D(WIN_WIDTH, WIN_HEIGHT); }

protected:

	//Events
	virtual void init() = 0;
	virtual void render(const vk::CommandBuffer& cmd, uint32_t swpIndex) = 0;
	virtual void destroy() = 0;
	virtual void key(const SDL_KeyboardEvent& event) {}

	using Buffer = std::vector<unsigned char>;

	//Utils
	std::exception error(const char* msg) { return std::exception(msg); }
	bool readContent(const char* path, Buffer& out);
	vk::ShaderModule loadModule(const char* path);
	uint32_t findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties);

private:

	void initSDL();
	void initVulkan();

	void baseInit();
	void baseDestroy();
	void nextFrame();
};

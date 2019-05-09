/*
	Application source
*/

#include <chrono>

#include <SDL.h>
#include <SDL_vulkan.h>

#include "App.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////

App::App() { }
App::~App() { }

void App::run()
{
	baseInit();

	try
	{
		bool running = true;
		while (running)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_QUIT:
					running = false;
					break;
				}
			}

			nextFrame();
		}
	}
	catch (const std::exception& e)
	{
		baseDestroy();
		throw e;
	}

	baseDestroy();
}

void App::baseDestroy()
{
	_device.waitIdle();

	this->destroy();

	for (int i = 0; i < FRAME_COUNT; i++)
	{
		_device.destroySemaphore(_frames[i].imageAvailable);
		_device.destroySemaphore(_frames[i].renderFinished);
		_device.destroyFence(_frames[i].fence);
	}
	//destroys all allocated command buffers too
	_device.destroyCommandPool(_commandPool);

	_device.destroySwapchainKHR(_swapchain);
	_instance.destroySurfaceKHR(_surface);
	_device.destroy();

	SDL_DestroyWindow(_window);
	SDL_Quit();
	_instance.destroy();
}

void App::baseInit()
{
	initSDL();
	log("SDL initialized successfully.");

	initVulkan();
	log("Vulkan initialized successfully.");

	this->init();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void App::initSDL()
{
	// Create an SDL window that supports Vulkan rendering.
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		throw error("Could not initialize SDL");

	_window = SDL_CreateWindow(
		"Vulkan Window",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		WIN_WIDTH,
		WIN_HEIGHT,
		SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI
	);

	if (_window == nullptr)
		throw error("Could not create SDL window");
}

void App::initVulkan()
{
	/*
		Setup vulkan instance
	*/

	//Get extensions
	uint32_t extensionCount = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(_window, &extensionCount, nullptr))
		throw error("SDL_Vulkan_GetInstanceExtensions failed");
	std::vector<const char*> extensionNames(extensionCount);
	if (!SDL_Vulkan_GetInstanceExtensions(_window, &extensionCount, extensionNames.data()))
		throw error("SDL_Vulkan_GetInstanceExtensions failed");

	//Get layers
	std::vector<const char*> layers;
#ifdef _DEBUG
	layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

	// Vulkan instance
	auto instanceInfo = vk::InstanceCreateInfo()
		.setEnabledExtensionCount((uint32_t)extensionNames.size())
		.setEnabledLayerCount((uint32_t)layers.size())
		.setPpEnabledExtensionNames(extensionNames.data())
		.setPpEnabledLayerNames(layers.data());

	_instance = vk::createInstance(instanceInfo);

	// Create a Vulkan surface for rendering
	VkSurfaceKHR c_surface;
	if (!SDL_Vulkan_CreateSurface(_window, _instance, &c_surface))
	{
		throw error("Could not create a Vulkan surface");
	}
	_surface = vk::SurfaceKHR(c_surface);

	/*
		Pick physical device and create logical device
	*/

	_queueFamilyIndex = -1;
	_pdevice = vk::PhysicalDevice();

	//pick physical device
	for (const auto& pd : _instance.enumeratePhysicalDevices())
	{
		int i = 0;
		for (const auto& family : pd.getQueueFamilyProperties())
		{
			//If surface is supported by this device
			if (pd.getSurfaceSupportKHR(i, _surface))
			{
				_pdevice = pd;
				_queueFamilyIndex = i;
				break;
			}

			i++;
		}

		//If a valid queue family was found
		if (_queueFamilyIndex > -1)
		{
			break;
		}
	}

	if (!_pdevice)
	{
		throw error("cannot find physical device matching criteria");
	}

	const char* deviceExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	const float queuePriorities[] = { 1.0f };

	auto queueCreateInfo = vk::DeviceQueueCreateInfo()
		.setQueueCount(1)
		.setQueueFamilyIndex(_queueFamilyIndex)
		.setPQueuePriorities(queuePriorities);

	auto deviceInfo = vk::DeviceCreateInfo()
		.setEnabledExtensionCount(1)
		.setPpEnabledExtensionNames(deviceExts)
		.setQueueCreateInfoCount(1)
		.setPQueueCreateInfos(&queueCreateInfo);

	_device = _pdevice.createDevice(deviceInfo);
	_queue = _device.getQueue(_queueFamilyIndex, 0);

	/*
		Create swapchain
	*/

	auto surfaceFormat = _pdevice.getSurfaceFormatsKHR(_surface).front();
	auto surfaceCaps = _pdevice.getSurfaceCapabilitiesKHR(_surface);

	// Determine number of images for swap chain
	uint32_t swapchainImageCount = surfaceCaps.minImageCount + 1;
	if (surfaceCaps.maxImageCount != 0 && swapchainImageCount > surfaceCaps.maxImageCount)
	{
		swapchainImageCount = surfaceCaps.maxImageCount;
	}

	if (!(surfaceCaps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst))
	{
		throw error("surface does not support usage as a transfer destination");
	}

	auto swapchainInfo = vk::SwapchainCreateInfoKHR()
		.setSurface(_surface)
		.setMinImageCount(swapchainImageCount)
		.setPresentMode(vk::PresentModeKHR::eFifo)
		.setImageColorSpace(surfaceFormat.colorSpace)
		.setImageFormat(surfaceFormat.format) //(vk::Format::eB8G8R8A8Unorm)
		.setImageExtent(surfaceCaps.currentExtent)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
		.setImageArrayLayers(1)
		.setImageSharingMode(vk::SharingMode::eExclusive)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPreTransform(surfaceCaps.currentTransform)
		.setClipped(true);

	_swapchain = _device.createSwapchainKHR(swapchainInfo);
	_swapchainImages = _device.getSwapchainImagesKHR(_swapchain);
	_swapchainFormat = swapchainInfo.imageFormat;

	/*
		Create command buffers
	*/

	auto cmdPoolInfo = vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _queueFamilyIndex);
	_commandPool = _device.createCommandPool(cmdPoolInfo);

	auto cmdBufferAllocInfo = vk::CommandBufferAllocateInfo(_commandPool, vk::CommandBufferLevel::ePrimary, FRAME_COUNT);
	auto commandBuffers = _device.allocateCommandBuffers(cmdBufferAllocInfo);

	for (int i = 0; i < FRAME_COUNT; i++)
	{
		_frames[i].commandBuffer = commandBuffers[i];
		_frames[i].imageAvailable = _device.createSemaphore(vk::SemaphoreCreateInfo());
		_frames[i].renderFinished = _device.createSemaphore(vk::SemaphoreCreateInfo());
		_frames[i].fence = _device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
	}
}

void App::nextFrame()
{
	uint32_t index = (_frameIndex++) % FRAME_COUNT;
	auto& frame = _frames[index];

	_device.waitForFences(1, &frame.fence, VK_TRUE, UINT64_MAX);
	_device.resetFences(1, &frame.fence);

	uint32_t imageIndex;
	_device.acquireNextImageKHR(_swapchain, UINT64_MAX, frame.imageAvailable, nullptr, &imageIndex);

	const auto& cmd = frame.commandBuffer;

	vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmd.begin(beginInfo);

	this->render(cmd, imageIndex);

	cmd.end();

	const vk::PipelineStageFlags dstStage[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	
	auto submitInfo = vk::SubmitInfo()
		//wait for image
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&frame.imageAvailable)
		.setPWaitDstStageMask(dstStage)
		//commands
		.setCommandBufferCount(1)
		.setPCommandBuffers(&frame.commandBuffer)
		//signal finish
		.setSignalSemaphoreCount(1)
		.setPSignalSemaphores(&frame.renderFinished);

	_queue.submit(1, &submitInfo, frame.fence);

	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&frame.renderFinished)
		.setSwapchainCount(1)
		.setPSwapchains(&_swapchain)
		.setPImageIndices(&imageIndex);

	_queue.presentKHR(&presentInfo);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

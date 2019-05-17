/*
	Application source
*/

#include <fstream>
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
		using namespace std::chrono;
		_lastUpdate = high_resolution_clock::now();

		bool running = true;
		while (running)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					key(event.key);
					break;
				case SDL_QUIT:
					running = false;
					break;
				}
			}

			//Log the number of fps
			auto now = high_resolution_clock::now();
			if ((now - _lastUpdate) > duration_cast<high_resolution_clock::duration>(std::chrono::seconds(1)))
			{
				Log::info("fps:", _fps);
				_lastUpdate = now;
				_fps = 0;
			}

			_fps++;
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
	Log::info("destroying instance...");
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
	_debug.destroy();

	SDL_DestroyWindow(_window);
	SDL_Quit();
	_instance.destroy();
	Log::info("instance destroyed");
}

void App::baseInit()
{
	initSDL();
	Log::info("SDL initialized successfully.");

	initVulkan();
	Log::info("Vulkan initialized successfully.");

	this->init();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void App::initSDL()
{
	// Create an SDL window that supports Vulkan rendering.
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		throw error("Could not initialize SDL");

	_window = SDL_CreateWindow(
		"Vulkan Raytracer",
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

	bool debug = true;

	//Get extensions
	uint32_t extensionCount = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(_window, &extensionCount, nullptr))
		throw error("SDL_Vulkan_GetInstanceExtensions failed");
	std::vector<const char*> extensions(extensionCount);
	if (!SDL_Vulkan_GetInstanceExtensions(_window, &extensionCount, extensions.data()))
		throw error("SDL_Vulkan_GetInstanceExtensions failed");

	//Get layers
	std::vector<const char*> layers;
	if (debug)
	{
		Log::info("running application in debug mode");
		layers.push_back("VK_LAYER_LUNARG_standard_validation");
		//layers.push_back("VK_LAYER_LUNARG_api_dump");
		extensions.push_back("VK_EXT_debug_report");
		extensions.push_back("VK_EXT_debug_utils");
	}

	// Vulkan instance
	auto instanceInfo = vk::InstanceCreateInfo()
		.setEnabledExtensionCount((uint32_t)extensions.size())
		.setEnabledLayerCount((uint32_t)layers.size())
		.setPpEnabledExtensionNames(extensions.data())
		.setPpEnabledLayerNames(layers.data());

	Log::info("create vulkan instance");
	_instance = vk::createInstance(instanceInfo);

	if (debug)
	{
		_debug.init(_instance);
	}
	
	// Create a Vulkan surface for rendering
	VkSurfaceKHR c_surface;
	if (!SDL_Vulkan_CreateSurface(_window, _instance, &c_surface))
	{
		throw error("could not create a Vulkan surface");
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
		auto props = pd.getProperties();
		Log::info("checking physical device: ", props.deviceName);
		switch (props.deviceType)
		{
		case vk::PhysicalDeviceType::eCpu:
			Log::info("device type: cpu");
			break;
		case vk::PhysicalDeviceType::eDiscreteGpu:
			Log::info("device type: discrete gpu");
			break;
		case vk::PhysicalDeviceType::eIntegratedGpu:
			Log::info("device type: integrated gpu");
			break;
		case vk::PhysicalDeviceType::eVirtualGpu:
			Log::info("device type: virtual gpu");
			break;
		case vk::PhysicalDeviceType::eOther:
			Log::info("device type: other");
			break;
		}

		int i = 0;
		for (const auto& family : pd.getQueueFamilyProperties())
		{
			Log::info("queue family", i, (uint32_t)family.queueFlags, family.queueCount);
			
			auto mask = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer;
			if ((family.queueFlags & mask) == mask)
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

bool App::readContent(const char* path, Buffer& out)
{
	std::ifstream f(path, std::ios::binary);
	if (f)
	{
		f.unsetf(std::ios::skipws);

		f.seekg(std::ios::end);
		auto size = f.tellg();
		f.seekg(std::ios::beg);

		out.reserve(size);
		out.insert(out.begin(), std::istream_iterator<unsigned char>(f),
			std::istream_iterator<unsigned char>());

		return true;
	}
	return false;
}

vk::ShaderModule App::loadModule(const char* path)
{
	Buffer spirv;
	if (!readContent(path, spirv))
		throw std::exception("unable to load shader file");

	return _device.createShaderModule(
		vk::ShaderModuleCreateInfo()
		.setPCode((const uint32_t*)spirv.data())
		.setCodeSize(spirv.size())
	);
}


uint32_t App::findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties)
{
	vk::PhysicalDeviceMemoryProperties mprops = _pdevice.getMemoryProperties();

	for (uint32_t i = 0; i < mprops.memoryTypeCount; i++)
	{
		if (typeBits & (1 << i))
		{
			return i;
		}
	}

	throw error("Could not find a suitable memory type");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

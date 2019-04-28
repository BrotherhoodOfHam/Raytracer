#include <iostream>
#include <array>
#include <chrono>

#include <SDL.h>
#include <SDL_vulkan.h>

//#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

#ifdef WIN32
#include <Windows.h>
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Application
{
	//SDL2
	SDL_Window* sdlWindow;

	//Vulkan
	vk::SurfaceKHR         surface;
	vk::SwapchainKHR       swapchain;
	vk::Instance           instance;
	vk::PhysicalDevice     pdevice;
	vk::Device             device;
	vk::Queue              queue;
	std::vector<vk::Image> swapchainImages;

	uint32_t queueFamilyIndex = 0;

	const static uint32_t FRAME_COUNT = 2;

	struct FrameContext
	{
		vk::CommandBuffer commandBuffer;
		vk::Fence         fence;
		vk::Semaphore     imageAvailable;
		vk::Semaphore     renderFinished;
	};

	uint32_t        frameIndex = 0;
	FrameContext    frames[FRAME_COUNT];
	vk::CommandPool commandPool;

	void initInstance()
	{
		//Get extensions
		uint32_t extensionCount = 0;
		if (!SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, nullptr))
			throw error("SDL_Vulkan_GetInstanceExtensions failed");
		std::vector<const char*> extensionNames(extensionCount);
		if (!SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, extensionNames.data()))
			throw error("SDL_Vulkan_GetInstanceExtensions failed");

		//Get layers
		std::vector<const char*> layers;
#ifdef _DEBUG
		layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

		// Vulkan instance
		auto instanceInfo = vk::InstanceCreateInfo()
			.setEnabledExtensionCount(extensionNames.size())
			.setEnabledLayerCount(layers.size())
			.setPpEnabledExtensionNames(extensionNames.data())
			.setPpEnabledLayerNames(layers.data());

		instance = vk::createInstance(instanceInfo);

		// Create a Vulkan surface for rendering
		VkSurfaceKHR c_surface;
		if (!SDL_Vulkan_CreateSurface(sdlWindow, instance, &c_surface))
		{
			throw error("Could not create a Vulkan surface");
		}
		surface = vk::SurfaceKHR(c_surface);
	}

	void initDevice()
	{
		auto phdevices = instance.enumeratePhysicalDevices();
		log("VkInstance::enumeratePhysicalDevices");

		//log physical devices
		for (auto phdevice : phdevices)
		{
			const char* devicetypes[] = {
				"other",
				"integrated gpu",
				"discrete gpu",
				"virtual gpu",
				"cpu"
			};

			auto props = phdevice.getProperties();
			log("deviceID:  ", props.deviceID);
			log("deviceName:", props.deviceName);
			log("deviceType:", devicetypes[(size_t)props.deviceType]);

			log("queues:");
			for (auto qprops : phdevice.getQueueFamilyProperties())
			{
				log("queue count:", qprops.queueCount);
				log("queue flags:",
					(qprops.queueFlags & vk::QueueFlagBits::eGraphics) ? "graphics" : "",
					(qprops.queueFlags & vk::QueueFlagBits::eCompute) ? "compute" : "",
					(qprops.queueFlags & vk::QueueFlagBits::eTransfer) ? "transfer" : "",
					(qprops.queueFlags & vk::QueueFlagBits::eProtected) ? "protected" : "",
					(qprops.queueFlags & vk::QueueFlagBits::eSparseBinding) ? "sparse" : ""
				);
			}
		}
		log();

		//pick physical device
		pdevice = phdevices[0];

		if (!pdevice.getSurfaceSupportKHR(queueFamilyIndex, surface))
		{
			throw error("Surface is not supported");
		}

		const char* deviceExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		const float queuePriorities[] = { 1.0f };

		auto queueCreateInfo = vk::DeviceQueueCreateInfo()
			.setQueueCount(1)
			.setQueueFamilyIndex(queueFamilyIndex)
			.setPQueuePriorities(queuePriorities);

		auto deviceInfo = vk::DeviceCreateInfo()
			.setEnabledExtensionCount(1)
			.setPpEnabledExtensionNames(deviceExts)
			.setQueueCreateInfoCount(1)
			.setPQueueCreateInfos(&queueCreateInfo);

		device = pdevice.createDevice(deviceInfo);
		queue = device.getQueue(queueFamilyIndex, 0);
	}

	void initSwapchain()
	{
		auto surfaceFormat = pdevice.getSurfaceFormatsKHR(surface).front();
		auto surfaceCaps = pdevice.getSurfaceCapabilitiesKHR(surface);

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
			.setSurface(surface)
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

		swapchain = device.createSwapchainKHR(swapchainInfo);
		swapchainImages = device.getSwapchainImagesKHR(swapchain);
	}

	void initRender()
	{
		auto cmdPoolInfo = vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queueFamilyIndex);
		commandPool = device.createCommandPool(cmdPoolInfo);

		auto cmdBufferAllocInfo = vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, FRAME_COUNT);
		auto commandBuffers = device.allocateCommandBuffers(cmdBufferAllocInfo);

		for (int i = 0; i < FRAME_COUNT; i++)
		{
			frames[i].commandBuffer = commandBuffers[i];
			frames[i].imageAvailable = device.createSemaphore(vk::SemaphoreCreateInfo());
			frames[i].renderFinished = device.createSemaphore(vk::SemaphoreCreateInfo());
			frames[i].fence = device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
		}
	}

	void draw()
	{
		uint32_t index = (frameIndex++) % FRAME_COUNT;
		auto& frame = frames[index];

		device.waitForFences(1, &frame.fence, VK_TRUE, UINT64_MAX);
		device.resetFences(1, &frame.fence);
		
		uint32_t imageIndex;
		device.acquireNextImageKHR(swapchain, UINT64_MAX, frame.imageAvailable, nullptr, &imageIndex);

		vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		frame.commandBuffer.begin(beginInfo);

		//////////////////////////////////////////////////

		auto subrscRange = vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(VK_REMAINING_MIP_LEVELS)
			.setBaseArrayLayer(0)
			.setLayerCount(VK_REMAINING_ARRAY_LAYERS);
		
		auto imageBarrier = vk::ImageMemoryBarrier(
			vk::AccessFlags(),
			vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			queueFamilyIndex,
			queueFamilyIndex,
			swapchainImages[imageIndex],
			subrscRange
		);

		frame.commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eTransfer,
			vk::DependencyFlags(),
			0, nullptr, 0, nullptr,
			1, &imageBarrier
		);

		//////////////////////////////////////////////////

		auto d = std::chrono::high_resolution_clock::now().time_since_epoch();
		auto t = std::chrono::duration_cast<std::chrono::milliseconds>(d).count() * 1000;
		auto v = sin((double)t * 3.14159);
		v = (v + 1.0) / 2;

		vk::ClearColorValue c;
		c.float32[0] = v;

		frame.commandBuffer.clearColorImage(
			swapchainImages[imageIndex],
			vk::ImageLayout::eTransferDstOptimal,
			&c,
			1,
			&subrscRange
		);

		//////////////////////////////////////////////////


		auto newImageBarrier = vk::ImageMemoryBarrier(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eMemoryRead,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			queueFamilyIndex,
			queueFamilyIndex,
			swapchainImages[imageIndex],
			subrscRange
		);

		frame.commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags(),
			0, nullptr, 0, nullptr,
			1, &newImageBarrier
		);

		//////////////////////////////////////////////////

		frame.commandBuffer.end();

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

		queue.submit(1, &submitInfo, frame.fence);

		auto presentInfo = vk::PresentInfoKHR()
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&frame.renderFinished)
			.setSwapchainCount(1)
			.setPSwapchains(&swapchain)
			.setPImageIndices(&imageIndex);

		queue.presentKHR(&presentInfo);
	}

	//SDL
	void initSDL()
	{
		// Create an SDL window that supports Vulkan rendering.
		if (SDL_Init(SDL_INIT_VIDEO) != 0)
			throw error("Could not initialize SDL");

		sdlWindow = SDL_CreateWindow(
			"Vulkan Window",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			1280,
			900,
			SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI
		);

		if (sdlWindow == nullptr)
			throw error("Could not create SDL window");
	}

	void init()
	{
		initSDL();
		log("SDL initialized successfully.");

		initInstance();
		initDevice();
		initSwapchain();
		initRender();
		log("Vulkan initialized successfully.");
	}

	//Utils
	std::exception error(const char* msg) { return std::exception(msg); }

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

public:

	Application()
	{
		init();
	}

	~Application()
	{
		device.waitIdle();

		for (int i = 0; i < FRAME_COUNT; i++)
		{
			device.destroySemaphore(frames[i].imageAvailable);
			device.destroySemaphore(frames[i].renderFinished);
			device.destroyFence(frames[i].fence);
		}
		//destroys all allocated command buffers too
		device.destroyCommandPool(commandPool);

		device.destroySwapchainKHR(swapchain);
		instance.destroySurfaceKHR(surface);
		device.destroy();

		SDL_DestroyWindow(sdlWindow);
		SDL_Quit();
		instance.destroy();
	}

	void run()
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

			draw(); 
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
#ifdef WIN32
	SetProcessDPIAware();
#endif

	try
	{
		Application app;
		app.run();
	}
	catch (std::exception e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	return 0;
}

/*
struct ResourceSet
{
	static vk::DescriptorSetLayoutCreateInfo layout()
	{
		vk::DescriptorSetLayoutBinding bindings[] = {
			// binding 0 is a UBO, array size 1, visible to all stages
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, VK_SHADER_STAGE_ALL_GRAPHICS, nullptr ),
			// binding 1 is a sampler, array size 1, visible to all stages
			vk::DescriptorSetLayoutBinding{ 1, VK_DESCRIPTOR_TYPE_SAMPLER,        1, VK_SHADER_STAGE_ALL_GRAPHICS, nullptr },
			// binding 5 is an image, array size 10, visible only to fragment shader
			vk::DescriptorSetLayoutBinding{ 5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		};

		vk::DescriptorSetLayoutCreateInfo i;
		i.

	}

	vk::DescriptorSet
};
*/
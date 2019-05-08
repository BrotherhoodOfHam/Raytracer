/*
	Startup
*/

#include "SDL_main.h"
#include "App.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

int main(int argc, char** argv)
{
#ifdef WIN32
	SetProcessDPIAware(); //workaround for windows 10
#endif

	try
	{
		App app;
		app.run();
	}
	catch (std::exception e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
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
/*
	Raytracer implementation
*/

#pragma once

#include "App.h"
#include "Camera.h"

class Raytracer : public App
{
	std::vector<vk::ImageView>   _swapchainViews;
	std::vector<vk::Framebuffer> _framebuffers;
	vk::RenderPass				 _renderPass;
	vk::PipelineLayout			 _pipelineLayout;
	vk::Pipeline				 _pipeline;
	vk::DescriptorSetLayout      _descriptorLayout; //uniform descriptor layout
	vk::DescriptorPool           _descriptorPool;	//uniform descriptor pool

	std::vector<vk::Buffer>        _uniformBuffers;
	std::vector<vk::DeviceMemory>  _uniformBuffersMemory;
	std::vector<vk::DescriptorSet> _descriptorSets;

	Camera _camera;

protected:

	void init() override;
	void render(const vk::CommandBuffer& cmd, uint32_t frame) override;
	void destroy() override;

	void key(const SDL_KeyboardEvent& event) override
	{
		_camera.key(event);
	}

private:
	
	void initDescriptorPool();
	void initFrame();
	void initPipeline();
	void initResources();
};

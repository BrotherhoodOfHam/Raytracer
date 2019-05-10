/*
	Raytracer implementation
*/

#pragma once

#include "App.h"

class Raytracer : public App
{
	std::vector<vk::ImageView>   _swapchainViews;
	std::vector<vk::Framebuffer> _framebuffers;
	vk::RenderPass				 _renderPass;
	vk::PipelineLayout			 _layout;
	vk::Pipeline				 _pipeline;
	
public:

	Raytracer() {}
	Raytracer(const Raytracer&) = delete;

	virtual void init() override;
	virtual void render(const vk::CommandBuffer& cmd, uint32_t frame) override;
	virtual void destroy() override;
};

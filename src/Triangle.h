/*
	Hello triangle implementation
*/

#pragma once

#include "App.h"

class Triangle : public App
{
private:

	std::vector<vk::ImageView>   _swapchainViews;
	std::vector<vk::Framebuffer> _framebuffers;
	vk::RenderPass				 _renderPass;
	vk::PipelineLayout			 _layout;
	vk::Pipeline				 _pipeline;

public:

	Triangle() {}
	Triangle(const Triangle&) = delete;

protected:

	virtual void init() override;
	virtual void render(const vk::CommandBuffer& cmd, uint32_t frame) override;
	virtual void destroy() override;
};
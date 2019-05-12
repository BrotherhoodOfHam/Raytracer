/*
	Raytracer implementation
*/

#include <chrono>

#include "Raytracer.h"

//Maths
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

////////////////////////////////////////////////////////////////////////////////////////

struct Uniforms
{
	glm::mat4 camera;
	float time;
	uint32_t framewidth;
	uint32_t frameheight;
};

////////////////////////////////////////////////////////////////////////////////////////

void Raytracer::init()
{
	initFrame();
	initDescriptorPool();
	initPipeline();
	initResources();
}

void Raytracer::initDescriptorPool()
{
	vk::DescriptorPoolSize poolSize;
	poolSize.descriptorCount = swapchainCount();
	poolSize.type = vk::DescriptorType::eUniformBuffer;

	vk::DescriptorPoolCreateInfo info;
	info.poolSizeCount = 1;
	info.pPoolSizes = &poolSize;
	info.maxSets = swapchainCount();

	_descriptorPool = device().createDescriptorPool(info);
}

void Raytracer::initFrame()
{
	//Render pass
	vk::AttachmentDescription colourAttachment;
	colourAttachment.format = swapchainFormat();
	colourAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colourAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colourAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colourAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colourAttachment.initialLayout = vk::ImageLayout::eUndefined;
	colourAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	vk::AttachmentReference attachmentRef;
	attachmentRef.attachment = 0;
	attachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpass;
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachmentRef;

	vk::RenderPassCreateInfo rinfo;
	rinfo.attachmentCount = 1;
	rinfo.pAttachments = &colourAttachment;
	rinfo.subpassCount = 1;
	rinfo.pSubpasses = &subpass;

	_renderPass = device().createRenderPass(rinfo);

	// Setup framebuffers
	for (const auto& img : swapchainImages())
	{
		vk::ImageSubresourceRange range;
		range.aspectMask = vk::ImageAspectFlagBits::eColor;
		range.levelCount = 1;
		range.layerCount = 1;
		
		auto info = vk::ImageViewCreateInfo()
			.setImage(img)
			.setViewType(vk::ImageViewType::e2D)
			.setSubresourceRange(range)
			.setFormat(swapchainFormat());

		_swapchainViews.push_back(device().createImageView(info));
		vk::ImageView attachments[] = { _swapchainViews[_swapchainViews.size() - 1] };

		vk::FramebufferCreateInfo fbinfo;
		fbinfo.renderPass = _renderPass;
		fbinfo.attachmentCount = 1;
		fbinfo.pAttachments = attachments;
		fbinfo.width = swapchainSize().width;
		fbinfo.height = swapchainSize().height;
		fbinfo.layers = 1;

		_framebuffers.push_back(device().createFramebuffer(fbinfo));
	}
}

void Raytracer::initPipeline()
{
	//Shaders
	auto vertex = loadModule("shaders/quad.vert.spv");
	auto fragment = loadModule("shaders/trace_sphere.frag.spv");

	vk::PipelineShaderStageCreateInfo stages[] = {
		vk::PipelineShaderStageCreateInfo()
			.setModule(vertex)
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eVertex),
		vk::PipelineShaderStageCreateInfo()
			.setModule(fragment)
			.setPName("main")
			.setStage(vk::ShaderStageFlagBits::eFragment),
	};

	//Vertex input layout
	vk::PipelineVertexInputStateCreateInfo vertexLayoutState;

	//Vertex input assembly
	vk::PipelineInputAssemblyStateCreateInfo assemblyState;
	assemblyState.topology = vk::PrimitiveTopology::eTriangleStrip;

	//Viewport
	vk::PipelineViewportStateCreateInfo viewportState;
	vk::Viewport viewport;
	vk::Rect2D scissor;
	viewportState.pViewports = &viewport;
	viewportState.viewportCount = 1;
	viewportState.pScissors = &scissor;
	viewportState.scissorCount = 1;

	//Rasterizer
	vk::PipelineRasterizationStateCreateInfo rasterState;
	rasterState.depthClampEnable = false;
	rasterState.rasterizerDiscardEnable = false;
	rasterState.polygonMode = vk::PolygonMode::eFill;
	rasterState.lineWidth = 1.0f;
	rasterState.cullMode = vk::CullModeFlagBits::eBack;
	rasterState.frontFace = vk::FrontFace::eClockwise;
	rasterState.depthBiasEnable = false;

	//Multisampling
	vk::PipelineMultisampleStateCreateInfo multisampling;
	multisampling.sampleShadingEnable = false;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

	//Colour blending attachment
	vk::PipelineColorBlendAttachmentState blendAttachState;
	blendAttachState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	blendAttachState.blendEnable = false;
	//Colour blending
	vk::PipelineColorBlendStateCreateInfo blendState;
	blendState.logicOpEnable = false;
	blendState.attachmentCount = 1;
	blendState.pAttachments = &blendAttachState;
	
	//Dynamic states
	vk::DynamicState dynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamicState;
	dynamicState.dynamicStateCount = 1;
	dynamicState.pDynamicStates = dynamicStates;

	///////////////////////////////////////////////////////////////////

	//Pipeline layout (uniform layout)
	vk::DescriptorSetLayoutBinding bindings[] = {
		vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics)
	};

	vk::DescriptorSetLayoutCreateInfo descriptorLayout;
	descriptorLayout.pBindings = bindings;
	descriptorLayout.bindingCount = 1;
	_descriptorLayout = device().createDescriptorSetLayout(descriptorLayout);

	vk::PipelineLayoutCreateInfo pipelineLayout;
	pipelineLayout.pSetLayouts = &_descriptorLayout;
	pipelineLayout.setLayoutCount = 1;

	_pipelineLayout = device().createPipelineLayout(pipelineLayout);

	///////////////////////////////////////////////////////////////////

	vk::GraphicsPipelineCreateInfo pinfo;
	pinfo.pStages = stages;
	pinfo.stageCount = 2;
	pinfo.layout = _pipelineLayout;
	pinfo.pColorBlendState = &blendState;
	pinfo.pInputAssemblyState = &assemblyState;
	pinfo.pMultisampleState = &multisampling;
	pinfo.pRasterizationState = &rasterState;
	pinfo.pVertexInputState = &vertexLayoutState;
	pinfo.pViewportState = &viewportState;
	pinfo.pDynamicState = &dynamicState;
	//pinfo.pDepthStencilState = 
	pinfo.renderPass = _renderPass;
	pinfo.subpass = 0;
	pinfo.basePipelineHandle = nullptr;
	pinfo.basePipelineIndex = -1;

	_pipeline = device().createGraphicsPipeline(nullptr, pinfo);
	device().destroyShaderModule(vertex);
	device().destroyShaderModule(fragment);
}

void Raytracer::initResources()
{
	_uniformBuffers.resize(swapchainCount());
	_uniformBuffersMemory.resize(swapchainCount());

	for (uint32_t i = 0; i < swapchainCount(); i++)
	{
		vk::BufferCreateInfo b;
		b.size = sizeof(Uniforms);
		b.usage = vk::BufferUsageFlagBits::eUniformBuffer;
		_uniformBuffers[i] = device().createBuffer(b);

		auto reqs = device().getBufferMemoryRequirements(_uniformBuffers[i]);

		vk::MemoryAllocateInfo alloc;
		alloc.allocationSize = reqs.size;
		alloc.memoryTypeIndex = findMemoryType(reqs.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		_uniformBuffersMemory[i] = device().allocateMemory(alloc);

		device().bindBufferMemory(_uniformBuffers[i], _uniformBuffersMemory[i], 0);
	}

	std::vector<vk::DescriptorSetLayout> layouts(swapchainCount(), _descriptorLayout);
	vk::DescriptorSetAllocateInfo allocSet;
	allocSet.descriptorPool = _descriptorPool;
	allocSet.descriptorSetCount = swapchainCount();
	allocSet.pSetLayouts = layouts.data();
	_descriptorSets = device().allocateDescriptorSets(allocSet);

	for (uint32_t i = 0; i < swapchainCount(); i++)
	{
		vk::DescriptorBufferInfo binfo;
		binfo.buffer = _uniformBuffers[i];
		binfo.offset = 0;
		binfo.range = sizeof(Uniforms);

		vk::WriteDescriptorSet write;
		write.dstSet = _descriptorSets[i];
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.descriptorCount = 1;
		write.descriptorType = vk::DescriptorType::eUniformBuffer;
		write.pBufferInfo = &binfo;

		device().updateDescriptorSets(
			vk::ArrayProxy<const vk::WriteDescriptorSet>(write),
			vk::ArrayProxy<const vk::CopyDescriptorSet>(nullptr)
		);
	}
}

////////////////////////////////////////////////////////////////////////////////////////

void Raytracer::render(const vk::CommandBuffer& cmd, uint32_t frame)
{
	_camera.update();

	//Update uniforms
	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now().time_since_epoch()
	).count();
	
	Uniforms u;
	u.camera = _camera.matrix();
	u.time = (float)(time % 1000000);
	u.framewidth = swapchainSize().width;
	u.frameheight = swapchainSize().height;
	
	void* ptr = device().mapMemory(_uniformBuffersMemory[frame], 0, sizeof(Uniforms));
	memcpy(ptr, &u, sizeof(Uniforms));
	device().unmapMemory(_uniformBuffersMemory[frame]);

	//Begin render pass
	vk::ClearValue cv;

	vk::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.renderPass = _renderPass;
	rpBeginInfo.framebuffer = _framebuffers[frame];
	rpBeginInfo.renderArea.offset = { 0,0 };
	rpBeginInfo.renderArea.extent = swapchainSize();
	rpBeginInfo.clearValueCount = 1;
	rpBeginInfo.pClearValues = &cv;

	cmd.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);

	//Update viewport
	vk::Viewport viewport;
	viewport.width = (float)swapchainSize().width;
	viewport.height = (float)swapchainSize().height;
	viewport.maxDepth = 1.0f;
	cmd.setViewport(0, 1, &viewport);
	vk::Rect2D scissor;
	scissor.extent = swapchainSize();
	cmd.setScissor(0, 1, &scissor);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipelineLayout, 0, 1, &_descriptorSets[0], 0, nullptr);
	cmd.draw(4, 1, 0, 0);

	cmd.endRenderPass();
}

////////////////////////////////////////////////////////////////////////////////////////

void Raytracer::destroy()
{
	const auto& d = device();
	d.destroyRenderPass(_renderPass);

	d.destroyDescriptorSetLayout(_descriptorLayout);
	d.destroyDescriptorPool(_descriptorPool);
	d.destroyPipeline(_pipeline);
	d.destroyPipelineLayout(_pipelineLayout);

	for (const auto& v : _swapchainViews)
		d.destroyImageView(v);
	for (const auto& f : _framebuffers)
		d.destroyFramebuffer(f);
	for (const auto& b : _uniformBuffers)
		d.destroyBuffer(b);
	for (const auto& b : _uniformBuffersMemory)
		d.freeMemory(b);
}

////////////////////////////////////////////////////////////////////////////////////////

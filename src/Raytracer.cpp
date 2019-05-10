/*
	Raytracer implementation
*/

#include "Raytracer.h"

void Raytracer::init()
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
	vk::PipelineLayoutCreateInfo pipelineLayout;

	_layout = device().createPipelineLayout(pipelineLayout);

	///////////////////////////////////////////////////////////////////

	vk::GraphicsPipelineCreateInfo pinfo;
	pinfo.pStages = stages;
	pinfo.stageCount = 2;
	pinfo.layout = _layout;
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

void Raytracer::render(const vk::CommandBuffer& cmd, uint32_t frame)
{
	vk::ClearValue cv;

	vk::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.renderPass = _renderPass;
	rpBeginInfo.framebuffer = _framebuffers[frame];
	rpBeginInfo.renderArea.offset = { 0,0 };
	rpBeginInfo.renderArea.extent = swapchainSize();
	rpBeginInfo.clearValueCount = 1;
	rpBeginInfo.pClearValues = &cv;

	cmd.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);

	vk::Viewport viewport;
	viewport.width = (float)swapchainSize().width;
	viewport.height = (float)swapchainSize().height;
	viewport.maxDepth = 1.0f;
	cmd.setViewport(0, 1, &viewport);
	vk::Rect2D scissor;
	scissor.extent = swapchainSize();
	cmd.setScissor(0, 1, &scissor);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline);
	cmd.draw(4, 1, 0, 0);

	cmd.endRenderPass();
}

void Raytracer::destroy()
{
	const auto& d = device();
	d.destroyRenderPass(_renderPass);

	d.destroyPipeline(_pipeline);
	d.destroyPipelineLayout(_layout);

	for (const auto& v : _swapchainViews)
	{
		d.destroyImageView(v);
	}
	for (const auto& f : _framebuffers)
	{
		d.destroyFramebuffer(f);
	}
}
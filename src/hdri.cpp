#include "hdri.h"

#include <iostream>
#include <array>

#include "glm/glm.hpp"

// 6 faces per cube, 2 traingles per face, 3 vertices per triangle
constexpr uint32_t NUM_VERTICES{ 6 * 2 * 3 };

/*
	TextureLoader::LoadTexture(frontTexturePath, width, height, numberOfChannels, &textureData[0]);
	TextureLoader::LoadTexture(backTexturePath, width, height, numberOfChannels, &textureData[1]);
	TextureLoader::LoadTexture(upTexturePath, width, height, numberOfChannels, &textureData[2]);
	TextureLoader::LoadTexture(downTexturePath, width, height, numberOfChannels, &textureData[3]);
	TextureLoader::LoadTexture(rightTexturePath, width, height, numberOfChannels, &textureData[4]);
	TextureLoader::LoadTexture(leftTexturePath, width, height, numberOfChannels, &textureData[5]);
*/

glm::vec3 vertexData[NUM_VERTICES]{
	// front
	{-1.0, -1.0, -1.0}, { 1.0, -1.0,  1.0}, {-1.0, -1.0,  1.0},
	{-1.0, -1.0, -1.0}, { 1.0, -1.0, -1.0}, { 1.0, -1.0,  1.0},

	// back
	{ 1.0,  1.0, -1.0}, {-1.0,  1.0,  1.0}, { 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0}, {-1.0,  1.0, -1.0}, {-1.0,  1.0,  1.0},

	// up
	{ 1.0, -1.0,  1.0}, {-1.0,  1.0,  1.0}, {-1.0, -1.0,  1.0},
	{ 1.0, -1.0,  1.0}, { 1.0,  1.0,  1.0}, {-1.0,  1.0,  1.0},

	// down
	{ 1.0,  1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0,  1.0, -1.0},
	{ 1.0,  1.0, -1.0}, { 1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0},

	// right
	{ 1.0, -1.0, -1.0}, { 1.0,  1.0,  1.0}, { 1.0, -1.0,  1.0},
	{ 1.0, -1.0, -1.0}, { 1.0,  1.0, -1.0}, { 1.0,  1.0,  1.0},

	// left
	{-1.0,  1.0, -1.0}, {-1.0, -1.0,  1.0}, {-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0, -1.0,  1.0},
};

VkFramebuffer create_e2c_framebuffer(VulkanEngine& engine, VkRenderPass renderpass, VkExtent2D extent, VkImageView attachment, VkFramebuffer* outFramebuffer)
{
	VkFramebufferCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.renderPass = renderpass;
	info.width = extent.width;
	info.height = extent.height;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;

	vkCreateFramebuffer(engine._device, &info, nullptr, outFramebuffer);
}

VkRenderPass create_e2c_renderpass(VulkanEngine& engine, VkFormat format, VkRenderPass* outRenderpass)
{
	VkAttachmentDescription attachment{};
	attachment.format = format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// don't care what previous layout was, but undefined means the content isn't
	// guaranteed to be preserved.
	// since we are clearing it then writing into it, it doesn't matter the previous layout
	// since the implementation will know the layout it writes to it
	attachment.initialLayout - VK_IMAGE_LAYOUT_UNDEFINED;
	// final layout is automatically transitioned to at end of renderpass
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference attachmentRef{};
	attachmentRef.attachment = 0;
	attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachmentRef;
	subpass.pDepthStencilAttachment = nullptr;

	VkRenderPassCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.pNext = nullptr;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 0;
	info.pDependencies = nullptr;

	vkCreateRenderPass(engine._device, &info, nullptr, outRenderpass);
}

void create_e2c_pipeline_layout(VulkanEngine& engine, VkDescriptorSetLayout layout, VkPipelineLayout* outLayout)
{
	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;
	info.setLayoutCount = 1;
	info.pSetLayouts = &layout;

	vkCreatePipelineLayout(engine._device, &info, nullptr, outLayout);
}

void create_e2c_pipeline(VulkanEngine& engine, VkPipelineLayout layout, VkPipeline* outPipeline)
{

}

AllocatedBuffer create_e2c_vertex_buffer(VulkanEngine& engine, size_t bufferSize, void* cpuArray)
{
	size_t bufferSize{  };
	AllocatedBuffer vertexBuffer{ engine.create_buffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU) };

	void* data;
	vmaMapMemory(engine._allocator, vertexBuffer._allocation, &data);
	std::memcpy(data, vertexData, bufferSize);
	vmaUnmapMemory(engine._allocator, vertexBuffer._allocation);
}

void create_e2c_cubemap(VulkanEngine& engine, VkExtent2D extent, VkFormat format, AllocatedImage* outCubemap)
{
	VkExtent3D extent3D{};
	extent3D.depth = 1;
	extent3D.height = extent.height;
	extent3D.width = extent.width;

	VkImageCreateInfo cubemapInfo{};
	cubemapInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	cubemapInfo.pNext = nullptr;
	cubemapInfo.arrayLayers = 6;
	cubemapInfo.extent = extent3D;
	cubemapInfo.format = format;
	cubemapInfo.imageType = VK_IMAGE_TYPE_2D;
	cubemapInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	cubemapInfo.mipLevels = 1;
	cubemapInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	cubemapInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	cubemapInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	vmaCreateImage(engine._allocator, &cubemapInfo, &allocInfo, &outCubemap->_image, &outCubemap->_allocation, nullptr);
}

AllocatedImage equirectangular_to_cubemap(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent)
{
	VkFormat hdriFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };

	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
	descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutInfo.pNext = nullptr;
	descriptorSetLayoutInfo.bindingCount = 1;
	descriptorSetLayoutInfo.pBindings = &binding;

	VkDescriptorSetLayout setLayout;
	VK_CHECK(vkCreateDescriptorSetLayout(engine._device, &descriptorSetLayoutInfo, nullptr, &setLayout));

	VkPipelineLayout pipelineLayout;
	create_e2c_pipeline_layout(engine, setLayout, &pipelineLayout);

	VkRenderPass renderpass;
	create_e2c_renderpass(engine, hdriFormat, &renderpass);

	VkPipeline pipeline;
	create_e2c_pipeline(engine, pipelineLayout, &pipeline);

	AllocatedImage cubemap;
	create_e2c_cubemap(engine, extent, hdriFormat, &cubemap);

	VkImageViewCreateInfo imageViewInfo{};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.pNext = nullptr;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.image = cubemap._image;
	imageViewInfo.format = hdriFormat;
	// using subresourceRange you can let the image view look at only particular layers of the image
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = 1;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageView imageView;
	VK_CHECK(vkCreateImageView(engine._device, &imageViewInfo, nullptr, &imageView));

	VkFramebuffer framebuffer;
	create_e2c_framebuffer(engine, renderpass, extent, imageView, &framebuffer);

	AllocatedBuffer vertexBuffer{ create_e2c_vertex_buffer(engine, NUM_VERTICES * sizeof(glm::vec3), vertexData) };


	engine.immediate_submit([=](VkCommandBuffer cmd) {
		VkClearValue clearValue{};
		clearValue.color = { {0.01, 0.01, 0.02, 1.0} };
		VkClearValue depthClear{};
		depthClear.depthStencil.depth = 1.0f;

		std::array<VkClearValue, 2> clearValues{ clearValue, depthClear };

		// start the main renderpass. we will use the clear color from above,
		// and the framebuffer corresponding to the index the swapchain gave us
		VkRenderPassBeginInfo rpInfo{};
		rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpInfo.pNext = nullptr;
		rpInfo.renderPass = renderpass;
		rpInfo.renderArea.offset.x = 0;
		rpInfo.renderArea.offset.y = 0;
		rpInfo.renderArea.extent = extent;
		rpInfo.framebuffer = framebuffer;
		rpInfo.clearValueCount = clearValues.size();
		rpInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

		// if this material has the same descriptor set layout then the pipelines might be the same and we don't have to rebind??
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &equirectangularSet, 0, nullptr);

		VkDeviceSize offset{ 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer._buffer, &offset);

		vkCmdDraw(cmd, NUM_VERTICES, 1, 0, 0);

		vkCmdEndRenderPass(cmd);
	});
}

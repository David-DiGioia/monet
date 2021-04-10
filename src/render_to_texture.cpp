#include "render_to_texture.h"

#include <iostream>
#include <array>

#include "vk_initializers.h"
#include "vk_textures.h"
#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"

// 6 faces per cube, 2 traingles per face, 3 vertices per triangle
constexpr uint32_t NUM_VERTICES_PLANE{ 2 * 3 };
constexpr uint32_t NUM_VERTICES_CUBE{ 6 * NUM_VERTICES_PLANE };
constexpr VkFormat DEPTH_FORMAT{ VK_FORMAT_D16_UNORM };

glm::vec3 vertexDataCube[NUM_VERTICES_CUBE]{
	// front
	{ 1.0, -1.0,  1.0}, {-1.0,  1.0,  1.0}, {-1.0, -1.0,  1.0},
	{ 1.0, -1.0,  1.0}, { 1.0,  1.0,  1.0}, {-1.0,  1.0,  1.0},

	// back
	{ 1.0,  1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0,  1.0, -1.0},
	{ 1.0,  1.0, -1.0}, { 1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0},

	// up
	{ 1.0,  1.0, -1.0}, {-1.0,  1.0,  1.0}, { 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0}, {-1.0,  1.0, -1.0}, {-1.0,  1.0,  1.0},

	// down
	{-1.0, -1.0, -1.0}, { 1.0, -1.0,  1.0}, {-1.0, -1.0,  1.0},
	{-1.0, -1.0, -1.0}, { 1.0, -1.0, -1.0}, { 1.0, -1.0,  1.0},

	// right
	{ 1.0, -1.0, -1.0}, { 1.0,  1.0,  1.0}, { 1.0, -1.0,  1.0},
	{ 1.0, -1.0, -1.0}, { 1.0,  1.0, -1.0}, { 1.0,  1.0,  1.0},

	// left
	{-1.0,  1.0, -1.0}, {-1.0, -1.0,  1.0}, {-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0, -1.0,  1.0},
};

glm::vec3 vertexDataPlane[NUM_VERTICES_PLANE]{
	{ 1.0, -1.0,  0.5}, {-1.0,  1.0,  0.5}, {-1.0, -1.0,  0.5},
	{ 1.0, -1.0,  0.5}, { 1.0,  1.0,  0.5}, {-1.0,  1.0,  0.5},
};

struct PushConstantData
{
	glm::mat4 rotMat;
	float roughness;
};

// Since Vulkan's coordinate system has y going down, we rotate the cube by 180 degrees in each case except up and down,
// in which case idk why they vulkan uses these orientations, I just matched them...
const glm::mat4 rotate180{ glm::rotate(glm::radians(180.0f), glm::vec3{0.0, 0.0, 1.0}) };
const glm::mat4 rotate90{ glm::rotate(glm::radians(90.0f), glm::vec3{0.0, 0.0, 1.0}) };
const glm::mat4 rotate270{ glm::rotate(glm::radians(270.0f), glm::vec3{0.0, 0.0, 1.0}) };

glm::mat4 rotationMatrices[6]{
	rotate180 * glm::mat4(1.0),													// front
	rotate180 * glm::rotate(glm::radians(180.0f), glm::vec3{0.0, 1.0, 0.0}),	// back
	rotate90 * glm::rotate(glm::radians(90.0f), glm::vec3{1.0, 0.0, 0.0}),		// up
	rotate270 * glm::rotate(glm::radians(-90.0f), glm::vec3{1.0, 0.0, 0.0}),	// down
	rotate180 * glm::rotate(glm::radians(-90.0f), glm::vec3{0.0, 1.0, 0.0}),	// right
	rotate180 * glm::rotate(glm::radians(90.0f), glm::vec3{0.0, 1.0, 0.0}),		// left
};

void create_rt_framebuffer(VulkanEngine& engine, VkRenderPass renderpass, VkExtent2D extent, VkImageView attachment, VkFramebuffer* outFramebuffer)
{
	VkFramebufferCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.renderPass = renderpass;
	info.width = extent.width;
	info.height = extent.height;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.layers = 1;

	vkCreateFramebuffer(engine._device, &info, nullptr, outFramebuffer);
}

void create_rt_renderpass(VulkanEngine& engine, VkFormat format, VkRenderPass* outRenderpass)
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
	attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

void create_rt_pipeline_layout(VulkanEngine& engine, VkDescriptorSetLayout layout, VkPipelineLayout* outLayout)
{
	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(PushConstantData);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	info.pushConstantRangeCount = 1;
	info.pPushConstantRanges = &pushConstant;
	info.setLayoutCount = 1;
	info.pSetLayouts = &layout;

	vkCreatePipelineLayout(engine._device, &info, nullptr, outLayout);
}

void create_rt_pipeline(VulkanEngine& engine, VkRenderPass renderpass, VkExtent2D extent, VkPipelineLayout layout, VkPipeline* outPipeline, const std::string& vertPath, const std::string& fragPath)
{
	std::string prefix{ "../../shaders/" };

	VkShaderModule vertShader;
	if (!engine.load_shader_module(prefix + vertPath, &vertShader)) {
		std::cout << "Error when building vertex shader module: " << (prefix + vertPath) << "\n";
	}

	VkShaderModule fragShader;
	if (!engine.load_shader_module(prefix + fragPath, &fragShader)) {
		std::cout << "Error when building fragment shader module: " << (prefix + fragPath) << "\n";
	}

	PipelineBuilder pipelineBuilder;
	// vertex input controls how to read vertices from vertex buffers
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	// input assembly is the configuration for drawing triangle lists, strips, or individual points
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	// build viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)extent.width;
	pipelineBuilder._viewport.height = (float)extent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;
	pipelineBuilder._scissor.offset = { 0, 0 };
	pipelineBuilder._scissor.extent = extent;
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkVertexInputAttributeDescription attribute{};
	attribute.binding = 0;
	attribute.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	attribute.location = 0;
	attribute.offset = 0;

	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDescription.stride = sizeof(glm::vec3);

	pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = 1;
	pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = &attribute;
	pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = 1;
	pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	pipelineBuilder._pipelineLayout = layout;

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertShader)
	);

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader)
	);

	*outPipeline = pipelineBuilder.build_pipeline(engine._device, renderpass);

	vkDestroyShaderModule(engine._device, vertShader, nullptr);
	vkDestroyShaderModule(engine._device, fragShader, nullptr);
}

AllocatedBuffer create_rt_vertex_buffer(VulkanEngine& engine, size_t bufferSize, void* cpuArray)
{
	AllocatedBuffer vertexBuffer{ engine.create_buffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU) };

	void* data;
	vmaMapMemory(engine._allocator, vertexBuffer._allocation, &data);
	std::memcpy(data, cpuArray, bufferSize);
	vmaUnmapMemory(engine._allocator, vertexBuffer._allocation);

	return vertexBuffer;
}

void create_rt_image(VulkanEngine& engine, VkExtent2D extent, VkFormat format, uint32_t mipLevels, bool isCubemap, AllocatedImage* outCubemap)
{
	VkExtent3D extent3D{};
	extent3D.depth = 1;
	extent3D.height = extent.height;
	extent3D.width = extent.width;

	VkImageCreateInfo cubemapInfo{};
	cubemapInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	cubemapInfo.pNext = nullptr;
	cubemapInfo.extent = extent3D;
	cubemapInfo.format = format;
	cubemapInfo.imageType = VK_IMAGE_TYPE_2D;
	cubemapInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	cubemapInfo.mipLevels = mipLevels;
	cubemapInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	cubemapInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	cubemapInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if (isCubemap) {
		cubemapInfo.arrayLayers = 6;
		cubemapInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	} else {
		cubemapInfo.arrayLayers = 1;
	}

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	vmaCreateImage(engine._allocator, &cubemapInfo, &allocInfo, &outCubemap->_image, &outCubemap->_allocation, nullptr);
}

Texture render_to_texture(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent, bool useMipmap, bool isCubemap, const std::string& vertPath, const std::string& fragPath)
{
	VkFormat hdriFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };

	uint32_t mipLevels{ useMipmap ? vkutil::get_mip_levels(extent.width, extent.height) : 1 };

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
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyDescriptorSetLayout(engine._device, setLayout, nullptr);
	});

	VkPipelineLayout pipelineLayout;
	create_rt_pipeline_layout(engine, setLayout, &pipelineLayout);
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyPipelineLayout(engine._device, pipelineLayout, nullptr);
	});

	VkRenderPass renderpass;
	create_rt_renderpass(engine, hdriFormat, &renderpass);
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyRenderPass(engine._device, renderpass, nullptr);
	});

	AllocatedImage allocatedImage;
	create_rt_image(engine, extent, hdriFormat, mipLevels, isCubemap, &allocatedImage);
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vmaDestroyImage(engine._allocator, allocatedImage._image, allocatedImage._allocation);
	});

	uint32_t numVertices{ isCubemap ? NUM_VERTICES_CUBE : NUM_VERTICES_PLANE };
	glm::vec3* vertexData{ isCubemap ? vertexDataCube : vertexDataPlane };
	AllocatedBuffer vertexBuffer{ create_rt_vertex_buffer(engine, numVertices * sizeof(glm::vec3), vertexData) };
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vmaDestroyBuffer(engine._allocator, vertexBuffer._buffer, vertexBuffer._allocation);
	});

	for (auto mipLevel{ 0 }; mipLevel < mipLevels; ++mipLevel) {
		VkPipeline pipeline;
		create_rt_pipeline(engine, renderpass, extent, pipelineLayout, &pipeline, vertPath, fragPath);
		engine._mainDeletionQueue.push_function([=, &engine]() {
			vkDestroyPipeline(engine._device, pipeline, nullptr);
		});

		uint32_t numLayers{ isCubemap ? 6u : 1u };
		for (auto layer{ 0 }; layer < numLayers; ++layer) {

			VkImageViewUsageCreateInfo viewUsage{};
			viewUsage.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
			viewUsage.pNext = nullptr;
			viewUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

			// This is a temporary image view only used as an attachment to the framebuffer
			VkImageViewCreateInfo attachmentViewInfo{};
			attachmentViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			attachmentViewInfo.pNext = &viewUsage;
			attachmentViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			attachmentViewInfo.image = allocatedImage._image;
			attachmentViewInfo.format = hdriFormat;
			// using subresourceRange you can let the image view look at only particular layers of the image
			attachmentViewInfo.subresourceRange.baseMipLevel = mipLevel;
			attachmentViewInfo.subresourceRange.levelCount = 1;
			attachmentViewInfo.subresourceRange.baseArrayLayer = layer;
			attachmentViewInfo.subresourceRange.layerCount = 1;
			attachmentViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			VkImageView attachmentView;
			VK_CHECK(vkCreateImageView(engine._device, &attachmentViewInfo, nullptr, &attachmentView));
			engine._mainDeletionQueue.push_function([=, &engine]() {
				vkDestroyImageView(engine._device, attachmentView, nullptr);
			});

			VkFramebuffer framebuffer;
			create_rt_framebuffer(engine, renderpass, extent, attachmentView, &framebuffer);
			engine._mainDeletionQueue.push_function([=, &engine]() {
				vkDestroyFramebuffer(engine._device, framebuffer, nullptr);
			});

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

				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &equirectangularSet, 0, nullptr);

				PushConstantData pushConstant{};
				// used in equirect_to_cubemap.vert
				pushConstant.rotMat = rotationMatrices[layer];
				// used in prefilter_environment.frag
				pushConstant.roughness = mipLevel / (mipLevels - 1.0f);

				vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData), &pushConstant);

				VkDeviceSize offset{ 0 };
				vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer._buffer, &offset);

				vkCmdDraw(cmd, numVertices, 1, 0, 0);

				vkCmdEndRenderPass(cmd);
			});
		}

		extent = vkutil::next_mip_level_extent(extent);
	}

	// This is the final image view, viewing all 6 layers of the image as a cube
	VkImageViewCreateInfo textureViewInfo{};
	textureViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	textureViewInfo.pNext = nullptr;
	textureViewInfo.image = allocatedImage._image;
	textureViewInfo.format = hdriFormat;
	textureViewInfo.subresourceRange.baseMipLevel = 0;
	textureViewInfo.subresourceRange.levelCount = mipLevels;
	textureViewInfo.subresourceRange.baseArrayLayer = 0;
	textureViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	if (isCubemap) {
		textureViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		textureViewInfo.subresourceRange.layerCount = 6;
	} else {
		textureViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		textureViewInfo.subresourceRange.layerCount = 1;
	}

	VkImageView textureView;
	VK_CHECK(vkCreateImageView(engine._device, &textureViewInfo, nullptr, &textureView));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyImageView(engine._device, textureView, nullptr);
	});

	Texture texture{};
	texture.image = allocatedImage;
	texture.imageView = textureView;
	texture.mipLevels = mipLevels;

	return texture;
}

// Shadow mapping -----------------------------------------------------------------------------

void prepareShadowMapRenderpass(VulkanEngine& engine, VkRenderPass* renderpass)
{
	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = DEPTH_FORMAT;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// We will read from depth, so it's important to store the depth attachment results
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// Attachment will be transitioned to shader read at render pass end
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentReference depthReference{};
	depthReference.attachment = 0;
	// Attachment will be used as depth/stencil during render pass
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies{};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pNext = nullptr;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &attachmentDescription;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassCreateInfo.pDependencies = dependencies.data();

	VK_CHECK(vkCreateRenderPass(engine._device, &renderPassCreateInfo, nullptr, renderpass));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyRenderPass(engine._device, *renderpass, nullptr);
	});
}

// Setup the offscreen framebuffer for rendering the scene from light's point-of-view to
// The depth attachment of this framebuffer will then be used to sample from in the fragment shader of the shadowing pass
void prepareShadowMapFramebuffer(VulkanEngine& engine, const ShadowGlobalResources& shadowGlobal, ShadowFrameResources* shadowFrame)
{
	// For shadow mapping we only need a depth attachment
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = shadowGlobal._width;
	imageInfo.extent.height = shadowGlobal._height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	// Depth stencil attachment
	imageInfo.format = DEPTH_FORMAT;
	// We will sample directly from the depth attachment for the shadow mapping
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VK_CHECK(vmaCreateImage(engine._allocator, &imageInfo, &allocInfo, &shadowFrame->depth.image._image, &shadowFrame->depth.image._allocation, nullptr));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vmaDestroyImage(engine._allocator, shadowFrame->depth.image._image, shadowFrame->depth.image._allocation);
	});

	VkImageViewCreateInfo depthStencilView{};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.pNext = nullptr;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = DEPTH_FORMAT;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;
	depthStencilView.image = shadowFrame->depth.image._image;
	VK_CHECK(vkCreateImageView(engine._device, &depthStencilView, nullptr, &shadowFrame->depth.imageView));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyImageView(engine._device, shadowFrame->depth.imageView, nullptr);
	});

	// Create sampler to sample from to depth attachment
	// Used to sample in the fragment shader for shadowed rendering
	VkFilter shadowmap_filter{ VK_FILTER_LINEAR };
	VkSamplerCreateInfo sampler{};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.pNext = nullptr;
	sampler.magFilter = shadowmap_filter;
	sampler.minFilter = shadowmap_filter;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(engine._device, &sampler, nullptr, &shadowFrame->depthSampler));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroySampler(engine._device, shadowFrame->depthSampler, nullptr);
	});

	// Create frame buffer
	VkFramebufferCreateInfo fbufCreateInfo{};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext = nullptr;
	fbufCreateInfo.renderPass = shadowGlobal._renderPass;
	fbufCreateInfo.attachmentCount = 1;
	fbufCreateInfo.pAttachments = &shadowFrame->depth.imageView;
	fbufCreateInfo.width = shadowGlobal._width;
	fbufCreateInfo.height = shadowGlobal._height;
	fbufCreateInfo.layers = 1;

	VK_CHECK(vkCreateFramebuffer(engine._device, &fbufCreateInfo, nullptr, &shadowFrame->frameBuffer));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyFramebuffer(engine._device, shadowFrame->frameBuffer, nullptr);
	});
}

void setupDescriptorSetLayouts(VulkanEngine& engine, std::array<VkDescriptorSetLayout, 2>& setLayoutsOut, VkPipelineLayout* pipelineLayout)
{
	// GLSL:
	//layout(set = 0, binding = 0) uniform LightBuffer {
	//	mat4 lightSpaceMatrix;
	//} lightData;
	VkDescriptorSetLayoutBinding lightBinding{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };

	VkDescriptorSetLayoutCreateInfo lightSetInfo{};
	lightSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lightSetInfo.pNext = nullptr;
	lightSetInfo.flags = 0;
	lightSetInfo.bindingCount = 1;
	lightSetInfo.pBindings = &lightBinding;

	// GLSL:
	//layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
	//	ObjectData objects[]; // SSBOs can only have unsized arrays
	//} objectBuffer;
	VkDescriptorSetLayoutBinding objectBinding{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };

	VkDescriptorSetLayoutCreateInfo objectSetInfo{};
	objectSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	objectSetInfo.pNext = nullptr;
	objectSetInfo.flags = 0;
	objectSetInfo.bindingCount = 1;
	objectSetInfo.pBindings = &objectBinding;

	vkCreateDescriptorSetLayout(engine._device, &lightSetInfo, nullptr, &setLayoutsOut[0]);
	vkCreateDescriptorSetLayout(engine._device, &objectSetInfo, nullptr, &setLayoutsOut[1]);
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyDescriptorSetLayout(engine._device, setLayoutsOut[1], nullptr);
		vkDestroyDescriptorSetLayout(engine._device, setLayoutsOut[0], nullptr);
	});

	//VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = setLayoutsOut.size();
	pipelineLayoutCreateInfo.pSetLayouts = setLayoutsOut.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VK_CHECK(vkCreatePipelineLayout(engine._device, &pipelineLayoutCreateInfo, nullptr, pipelineLayout));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyPipelineLayout(engine._device, *pipelineLayout, nullptr);
	});
}

// Only sets up the light uniform buffer descriptor set, since the objects one is already set up
void setupDescriptorSets(VulkanEngine& engine, ShadowFrameResources& shadowFrame, VkBuffer& objectBuffer, std::array<VkDescriptorSetLayout, 2>& setLayouts)
{
	// Image descriptor for the shadow map attachment
	VkDescriptorImageInfo shadowMapDescriptor{};
	shadowMapDescriptor.sampler = shadowFrame.depthSampler;
	shadowMapDescriptor.imageView = shadowFrame.depth.imageView;
	// imageLayout is which layout it will be in already, it won't change it to this for us (the renderpass transitiosn it to this for us)
	shadowMapDescriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkDescriptorSetAllocateInfo allocInfoLight{};
	allocInfoLight.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfoLight.descriptorPool = engine._descriptorPool;
	allocInfoLight.descriptorSetCount = 1;
	allocInfoLight.pSetLayouts = &setLayouts[0];

	VkDescriptorSetAllocateInfo allocInfoObjects{};
	allocInfoObjects.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfoObjects.descriptorPool = engine._descriptorPool;
	allocInfoObjects.descriptorSetCount = 1;
	allocInfoObjects.pSetLayouts = &setLayouts[1];

	VK_CHECK(vkAllocateDescriptorSets(engine._device, &allocInfoLight, &shadowFrame._shadowDescriptorSetLight));
	VK_CHECK(vkAllocateDescriptorSets(engine._device, &allocInfoObjects, &shadowFrame._shadowDescriptorSetObjects));

	VkDescriptorBufferInfo lightInfo{};
	lightInfo.offset = 0;
	lightInfo.range = VK_WHOLE_SIZE;
	lightInfo.buffer = shadowFrame._shadowLightBuffer._buffer;

	VkDescriptorBufferInfo objectInfo{};
	objectInfo.offset = 0;
	objectInfo.range = VK_WHOLE_SIZE;
	objectInfo.buffer = objectBuffer;

	std::vector<VkWriteDescriptorSet> writeDescriptorSets{
		// Set 0, Binding 0 : Vertex shader uniform buffer (LightBuffer)
		vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shadowFrame._shadowDescriptorSetLight, &lightInfo, 0),
		// Set 1, Binding 0 : Object SSBO
		vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shadowFrame._shadowDescriptorSetObjects, &objectInfo, 0),
	};

	vkUpdateDescriptorSets(engine._device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void initShadowPipeline(VulkanEngine& engine, VkRenderPass& renderpass, VkPipelineLayout pipelineLayout, VkPipeline* pipeline)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{ vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) };

	VkPipelineRasterizationStateCreateInfo rasterizationStateCI{ vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL) };

	VkPipelineColorBlendAttachmentState blendAttachmentState{ vkinit::color_blend_attachment_state() };

	VkPipelineColorBlendStateCreateInfo colorBlendStateCI{};
	colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCI.pNext = nullptr;
	colorBlendStateCI.logicOpEnable = VK_FALSE;
	colorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCI.attachmentCount = 1;
	colorBlendStateCI.pAttachments = &blendAttachmentState;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{ vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL) };


	// Note that we don't have any scissors or viewports since we set VK_DYNAMIC_STATE_VIEWPORT flag in VkPipelineDynamicStateCreateInfo, which means we set it
	// dynamically with vkCmdSetViewport 
	VkPipelineViewportStateCreateInfo viewportStateCI{};
	viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCI.scissorCount = 1;
	viewportStateCI.pScissors = nullptr;
	viewportStateCI.viewportCount = 1;
	viewportStateCI.pViewports = nullptr;

	VkPipelineMultisampleStateCreateInfo multisampleStateCI{};
	multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };

	std::string prefix{ "../../shaders/" };
	std::string vertPath{ prefix + "depth.vert.spv" };
	VkShaderModule vertShader;
	if (!engine.load_shader_module(vertPath, &vertShader)) {
		std::cout << "Error when building vertex shader module: " << vertPath << "\n";
	}
	VkPipelineShaderStageCreateInfo vertStage{ vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertShader) };

	// vertex input controls how to read vertices from vertex buffers
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{ vkinit::vertex_input_state_create_info() };
	// input assembly is the configuration for drawing triangle lists, strips, or individual points
	VertexInputDescription vertexDescription{ Vertex::get_vertex_description() };
	vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();
	vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();

	rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;

	// No blend attachment states (no color attachments used)
	colorBlendStateCI.attachmentCount = 0;
	// Cull front faces
	depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	// Enable depth bias
	rasterizationStateCI.depthBiasEnable = VK_TRUE;
	// Add depth bias to dynamic state, so we can change it at runtime
	VkPipelineDynamicStateCreateInfo dynamicStateCI{};
	dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCI.dynamicStateCount = (uint32_t)dynamicStateEnables.size();
	dynamicStateCI.pDynamicStates = dynamicStateEnables.data();

	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.layout = pipelineLayout;
	pipelineCI.renderPass = renderpass;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.stageCount = 1;
	pipelineCI.pStages = &vertStage;
	pipelineCI.pVertexInputState = &vertexInputInfo;
	pipelineCI.pDynamicState = &dynamicStateCI;
	pipelineCI.renderPass = renderpass;

	VK_CHECK(vkCreateGraphicsPipelines(engine._device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, pipeline));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyPipeline(engine._device, *pipeline, nullptr);
	});

	vkDestroyShaderModule(engine._device, vertShader, nullptr);
}


#include "vk_engine.h"

#include <iostream>
#include <cmath>
#include <fstream>
#include <cstring> // memcpy
#include <array>
#include <sstream>
#include <fstream>

#include "SDL.h"
#include "SDL_vulkan.h"
#include "vk_types.h"
#include "vk_initializers.h"
#include "VkBootstrap.h"
#include "glm/gtx/transform.hpp"
#include "vk_textures.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define VK_CHECK(x)\
	do\
	{\
		VkResult err = x;\
		if (err) {\
			std::cout << "Detected Vulkan error: " << err << '\n';\
			abort();\
		}\
	} while (0)\

bool RenderObject::operator<(const RenderObject& other) const
{
	if (material->pipeline == other.material->pipeline) {
		return mesh->_vertexBuffer._buffer < other.mesh->_vertexBuffer._buffer;
	} else {
		return material->pipeline < other.material->pipeline;
	}
}

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags{ (SDL_WindowFlags)(SDL_WINDOW_VULKAN) };

	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);

	init_vulkan();
	init_swapchain();
	init_commands();
	init_default_renderpass();
	init_framebuffers();
	init_sync_structures();
	init_descriptors(); // descriptors are needed at pipeline create, so before materials
	load_images();
	load_materials();
	load_meshes();
	init_scene();

	// everything went fine
	_isInitialized = true;
}

void VulkanEngine::init_scene()
{
	// minecraft -------------------------------------------------------------------

	// create a sampler for the texture
	VkSamplerCreateInfo samplerInfo{ vkinit::sampler_create_info(VK_FILTER_NEAREST) };
	VkSampler blockySampler;
	vkCreateSampler(_device, &samplerInfo, nullptr, &blockySampler);

	_mainDeletionQueue.push_function([=]() {
		vkDestroySampler(_device, blockySampler, nullptr);
	});

	Material* mat{ get_material("textured_lit") };

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = _descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &_singleTextureSetLayout;

	vkAllocateDescriptorSets(_device, &allocInfo, &mat->textureSet);

	VkDescriptorImageInfo imageBufferInfo{};
	imageBufferInfo.sampler = blockySampler;
	imageBufferInfo.imageView = _loadedTextures["empire_diffuse"].imageView;
	imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet texture1{ vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mat->textureSet, &imageBufferInfo, 0) };

	vkUpdateDescriptorSets(_device, 1, &texture1, 0, nullptr);




	_camPos = glm::vec3{ 0.0f, 6.0f, 10.0f };
	RenderObject minecraft{};
	minecraft.mesh = get_mesh("minecraft");
	minecraft.material = get_material("textured_lit");
	minecraft.transformMatrix = glm::translate(glm::mat4{ 1.0 }, glm::vec3(0.0, 0.0, 0.0));
	_renderables.insert(minecraft);

	// penguin monkey --------------------------------------------------------------

	//_camPos = glm::vec3{ 0.0f, 6.0f, 10.0f };

	//RenderObject monkey{};
	//monkey.mesh = get_mesh("monkey");
	//monkey.material = get_material("default");
	//monkey.transformMatrix = glm::translate(glm::mat4{ 1.0 }, glm::vec3(0.0, 5.0, 0.0));

	//_renderables.insert(monkey);

	//uint32_t idx{ 0 };

	//for (auto x{ -40 }; x <= 40; ++x) {
	//	for (auto y{ -40 }; y <= 40; ++y) {
	//		RenderObject obj{};
	//		glm::mat4 scale;
	//		if (idx % 2) {
	//			obj.mesh = get_mesh("monkey");
	//			scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
	//		} else {
	//			obj.mesh = get_mesh("penguin");
	//			scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(1.0, 1.0, 1.0));
	//		}
	//		if (idx % 3 == 0) {
	//			obj.material = get_material("default");
	//		} else if (idx % 3 == 1) {
	//			obj.material = get_material("textured_lit");
	//		} else {
	//			obj.material = get_material("green");
	//		}
	//		glm::mat4 translation{ glm::translate(glm::mat4{1.0}, glm::vec3(x, 0, y)) };
	//		obj.transformMatrix = translation * scale;

	//		_renderables.insert(obj);
	//		++idx;
	//	}
	//}
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	// allocate the default command buffer that we will use for the instant commands
	VkCommandBufferAllocateInfo cmdAllocInfo{ vkinit::command_buffer_allocate_info(_uploadContext._commandPool, 1) };
	
	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &cmd));

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo{};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// execute the function
	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	// submit command buffer to the queue and execute it.
	// _uploadFence will not block until the graphics commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _uploadContext._uploadFence));

	vkWaitForFences(_device, 1, &_uploadContext._uploadFence, VK_TRUE, 9999999999);
	vkResetFences(_device, 1, &_uploadContext._uploadFence);

	// clear the command pool. This will free the command buffer too
	vkResetCommandPool(_device, _uploadContext._commandPool, 0);
}

// upload to GPU
void VulkanEngine::upload_mesh(Mesh& mesh)
{
	const size_t bufferSize{ mesh._vertices.size() * sizeof(Vertex) };
	// allocate staging buffer
	VkBufferCreateInfo stagingBufferInfo{};
	stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferInfo.pNext = nullptr;
	stagingBufferInfo.size = bufferSize;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	// let the VMA library know that this data should be on CPU RAM
	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingBuffer{};
	VK_CHECK(vmaCreateBuffer(_allocator, &stagingBufferInfo, &vmaAllocInfo,
		&stagingBuffer._buffer,
		&stagingBuffer._allocation,
		nullptr));

	// copy vertex data
	void* data;
	vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
	std::memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(_allocator, stagingBuffer._allocation);

	// now we need the GPU-side buffer since we've populated the staging buffer
	VkBufferCreateInfo vertexBufferInfo{};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.pNext = nullptr;
	vertexBufferInfo.size = bufferSize;
	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	// let the VMA library know that this data should be GPU native
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VK_CHECK(vmaCreateBuffer(_allocator, &vertexBufferInfo, &vmaAllocInfo,
		&mesh._vertexBuffer._buffer,
		&mesh._vertexBuffer._allocation,
		nullptr));

	immediate_submit([=](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer._buffer, mesh._vertexBuffer._buffer, 1, &copy);
	});

	_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer, mesh._vertexBuffer._allocation);
	});
	vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation);
}

void VulkanEngine::load_images()
{
	Texture lostEmpire;
	vkutil::load_image_from_file(*this, "../../assets/lost_empire-RGBA.png", lostEmpire.image);

	VkImageViewCreateInfo imageInfo{ vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_UNORM, lostEmpire.image._image, VK_IMAGE_ASPECT_COLOR_BIT) };
	vkCreateImageView(_device, &imageInfo, nullptr, &lostEmpire.imageView);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, lostEmpire.imageView, nullptr);
	});

	_loadedTextures["empire_diffuse"] = lostEmpire;
}

void VulkanEngine::load_meshes()
{
	std::string name, path;
	std::string prefix{ "../../assets/" };
	std::ifstream file{ "../../assets/_load_meshes.txt" };
	while (file >> name >> path) {
		load_mesh(name, prefix + path);
	}
}


// load mesh onto CPU then upload it to the GPU
void VulkanEngine::load_mesh(const std::string& name, const std::string& path)
{
	Mesh mesh{};
	mesh.load_from_obj(path);

	// make sure mesh is sent to GPU
	upload_mesh(mesh);

	// note that we are copying them. Eventually we'll delete the
	// hardcoded monkey and triangle so it's no problem for now
	_meshes[name] = mesh;
}

void VulkanEngine::load_materials()
{
	std::string name, vertPath, fragPath;
	std::string prefix{ "../../shaders/" };
	std::ifstream file{ "../../shaders/_load_materials.txt" };
	while (file >> name >> vertPath >> fragPath) {
		init_pipeline(name, prefix + vertPath, prefix + fragPath);
	}
}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize)
{
	// Calculate required alignment based on minimum device offset alignment
	size_t minUboAlignment{ _gpuProperties.limits.minUniformBufferOffsetAlignment };
	size_t alignedSize{ originalSize };
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

void VulkanEngine::init_descriptors()
{
	std::vector<VkDescriptorPoolSize> sizes{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 }
	};

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = 10;
	pool_info.poolSizeCount = (uint32_t)sizes.size();
	pool_info.pPoolSizes = sizes.data();

	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptorPool));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
	});

	VkDescriptorSetLayoutBinding cameraBind{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };
	VkDescriptorSetLayoutBinding sceneBind{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1) };

	std::array<VkDescriptorSetLayoutBinding, 2> bindings{ cameraBind, sceneBind };

	VkDescriptorSetLayoutCreateInfo setInfo{};
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.pNext = nullptr;
	setInfo.flags = 0;
	setInfo.bindingCount = bindings.size();
	setInfo.pBindings = bindings.data();

	VkDescriptorSetLayoutBinding objectBind{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };

	VkDescriptorSetLayoutCreateInfo setInfo2{};
	setInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo2.pNext = nullptr;
	setInfo2.flags = 0;
	setInfo2.bindingCount = 1;
	setInfo2.pBindings = &objectBind;

	VkDescriptorSetLayoutBinding textureBind{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0) };

	VkDescriptorSetLayoutCreateInfo setInfo3{};
	setInfo3.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo3.pNext = nullptr;
	setInfo3.flags = 0;
	setInfo3.bindingCount = 1;
	setInfo3.pBindings = &textureBind;

	VK_CHECK(vkCreateDescriptorSetLayout(_device, &setInfo, nullptr, &_globalSetLayout));
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &setInfo2, nullptr, &_objectSetLayout));
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &setInfo3, nullptr, &_singleTextureSetLayout));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _singleTextureSetLayout, nullptr);
	});

	const size_t sceneParamBufferSize{ FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData)) };
	_sceneParameterBuffer = create_buffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(_allocator, _sceneParameterBuffer._buffer, _sceneParameterBuffer._allocation);
	});

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		const uint32_t MAX_OBJECTS{ 10000 };

		_frames[i].objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_frames[i].cameraBuffer = create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(_allocator, _frames[i].objectBuffer._buffer, _frames[i].objectBuffer._allocation);
			vmaDestroyBuffer(_allocator, _frames[i].cameraBuffer._buffer, _frames[i].cameraBuffer._allocation);
		});

		// allocate one descriptor set for each frame
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.pNext = nullptr;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &_globalSetLayout;

		// allocate the descriptor set that will point to object buffer
		VkDescriptorSetAllocateInfo objectSetAlloc{};
		objectSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		objectSetAlloc.pNext = nullptr;
		objectSetAlloc.descriptorPool = _descriptorPool;
		objectSetAlloc.descriptorSetCount = 1;
		objectSetAlloc.pSetLayouts = &_objectSetLayout;

		VK_CHECK(vkAllocateDescriptorSets(_device, &allocInfo, &_frames[i].globalDescriptor));
		VK_CHECK(vkAllocateDescriptorSets(_device, &objectSetAlloc, &_frames[i].objectDescriptor));

		// information about the buffer we want to point at in the descriptor
		VkDescriptorBufferInfo cameraInfo{};
		cameraInfo.buffer = _frames[i].cameraBuffer._buffer;
		cameraInfo.offset = 0;
		cameraInfo.range = sizeof(GPUCameraData);

		VkDescriptorBufferInfo sceneInfo{};
		sceneInfo.buffer = _sceneParameterBuffer._buffer;
		// store scene data for all scenes in the same buffer. camera data is in separate buffer
		sceneInfo.offset = 0;
		sceneInfo.range = sizeof(GPUSceneData);

		VkDescriptorBufferInfo objectInfo{};
		objectInfo.buffer = _frames[i].objectBuffer._buffer;
		objectInfo.offset = 0;
		objectInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;

		VkWriteDescriptorSet cameraWrite{ vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _frames[i].globalDescriptor, &cameraInfo, 0) };
		VkWriteDescriptorSet sceneWrite{ vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _frames[i].globalDescriptor, &sceneInfo, 1) };
		VkWriteDescriptorSet objectWrite{ vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _frames[i].objectDescriptor, &objectInfo, 0) };
		std::array<VkWriteDescriptorSet, 3> setWrites{ cameraWrite, sceneWrite, objectWrite };
		vkUpdateDescriptorSets(_device, setWrites.size(), setWrites.data(), 0, nullptr);
	}
}

// name must be unique!
void VulkanEngine::init_pipeline(const std::string& name, const std::string& vertPath, const std::string& fragPath)
{
	VkShaderModule meshVertShader;
	if (!load_shader_module(vertPath, &meshVertShader)) {
		std::cout << "Error when building vertex shader module\n";
	} else {
		std::cout << "Vertex shader successfully loaded\n";
	}

	VkShaderModule triangleFragShader;
	if (!load_shader_module(fragPath, &triangleFragShader)) {
		std::cout << "Error when building fragment shader module\n";
	} else {
		std::cout << "Fragment shader successfully loaded\n";
	}

	VkPipelineLayoutCreateInfo pipeline_layout_info{ vkinit::pipeline_layout_create_info() };
	VkPushConstantRange push_constant{};
	// this push constant range starts at the beginning
	push_constant.offset = 0;
	push_constant.size = sizeof(MeshPushConstants);
	// this push constant range is accessible only in the vertex shader
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::array<VkDescriptorSetLayout, 3> setLayouts{ _globalSetLayout, _objectSetLayout, _singleTextureSetLayout };

	pipeline_layout_info.pPushConstantRanges = &push_constant;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.setLayoutCount = setLayouts.size();
	pipeline_layout_info.pSetLayouts = setLayouts.data();

	VkPipelineLayout layout;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &layout));

	PipelineBuilder pipelineBuilder;
	// vertex input controls how to read vertices from vertex buffers
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	// input assembly is the configuration for drawing triangle lists, strips, or individual points
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	// build viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;
	pipelineBuilder._scissor.offset = { 0, 0 };
	pipelineBuilder._scissor.extent = _windowExtent;
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	VertexInputDescription vertexDescription{ Vertex::get_vertex_description() };
	// connect the pipeline builder vertex input info to the one we get from Vertex
	pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();
	pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineBuilder._pipelineLayout = layout;

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader)
	);

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader)
	);

	VkPipeline pipeline{ pipelineBuilder.build_pipeline(_device, _renderPass) };

	create_material(pipeline, layout, name);

	vkDestroyShaderModule(_device, meshVertShader, nullptr);
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipeline(_device, pipeline, nullptr);
		vkDestroyPipelineLayout(_device, layout, nullptr);
	});
}

void VulkanEngine::init_sync_structures()
{
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	// we want to create the fence with the Create Signaled flag so we
	// can wait on it before using it on a gpu command (for the first frame)
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	VkFenceCreateInfo uploadFenceCreateInfo{};
	uploadFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	uploadFenceCreateInfo.pNext = nullptr;

	VK_CHECK(vkCreateFence(_device, &uploadFenceCreateInfo, nullptr, &_uploadContext._uploadFence));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _uploadContext._uploadFence, nullptr);
	});

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._presentSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(_device, _frames[i]._presentSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
		});
	}
}

void VulkanEngine::init_default_renderpass()
{
	// the renderpass will use this color attachment
	VkAttachmentDescription color_attachment{};
	// the attachment will have the format needed by the swapchain
	color_attachment.format = _swapchainImageFormat;
	// 1 sample, we won't be doing MSAA
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// we clear when this attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// we keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// we don't care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// we don't know or care about the starting layout of the attachment
	color_attachment.initialLayout - VK_IMAGE_LAYOUT_UNDEFINED;
	// after the renderpass ends, the image has to be on a layout ready for display
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref{};
	// attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	// the driver will transition to this layout for us as the start of this subpass
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depth_attachment{};
	depth_attachment.flags = 0;
	depth_attachment.format = _depthFormat;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref{};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	std::array<VkAttachmentDescription, 2> attachments{ color_attachment, depth_attachment };

	VkRenderPassCreateInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = attachments.size();
	render_pass_info.pAttachments = attachments.data();
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(_device, _renderPass, nullptr);
	});
}

void VulkanEngine::init_framebuffers()
{
	// the framebuffer will connect the renderpass to the images for rendering
	VkFramebufferCreateInfo fb_info{};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;
	fb_info.renderPass = _renderPass;
	fb_info.width = _windowExtent.width;
	fb_info.height = _windowExtent.height;
	fb_info.layers = 1;

	// grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount{ (uint32_t)_swapchainImages.size() };
	_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	// create framebuffers for each of the swapchain image views
	for (auto i{ 0 }; i < swapchain_imagecount; ++i) {

		std::array<VkImageView, 2> attachments{
			_swapchainImageViews[i],
			_depthImageView
		};

		fb_info.attachmentCount = attachments.size();
		fb_info.pAttachments = attachments.data();
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
		});
	}
}

void VulkanEngine::init_commands()
{
	// create a command pool for commands submitted to the graphics queue
	// we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo{ vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) };
	VkCommandPoolCreateInfo uploadCommandPoolInfo{ vkinit::command_pool_create_info(_graphicsQueueFamily) };

	// create pool for upload context
	VK_CHECK(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_uploadContext._commandPool));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(_device, _uploadContext._commandPool, nullptr);
	});

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo{ vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1) };
		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
		});
	}
}

void VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU, _device, _surface };

	vkb::Swapchain vkbSwapchain{ swapchainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // use vsync present mode
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.build()
		.value() };

	// store the swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
	_swapchainImageFormat = vkbSwapchain.image_format;

	_mainDeletionQueue.push_function([=]() {
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	});

	// depth image size will match the window
	VkExtent3D depthImageExtent{
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	// hardcoding the depth format to the 32 bit float
	_depthFormat = VK_FORMAT_D32_SFLOAT;

	// the depth image will be an image with the format we selected and Depth Attachment usage flag
	VkImageCreateInfo dimg_info{ vkinit::image_create_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent) };

	// for the depth image, we want to allocate it from GPU local memory
	VmaAllocationCreateInfo dimg_allocinfo{};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dimg_allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr);
	// build an imageView for the depth image to use for rendering
	VkImageViewCreateInfo dview_info{ vkinit::imageview_create_info(_depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT) };

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

	// add to deletion queues
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _depthImageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
	});
}

void VulkanEngine::init_vulkan()
{
	vkb::InstanceBuilder builder;

	auto inst_ret{ builder.set_app_name("Example Vulkan Application")
		.request_validation_layers(true)
		.require_api_version(1, 1, 0)
		.use_default_debug_messenger()
		.build() };

	vkb::Instance vkb_inst{ inst_ret.value() };

	// store the instance
	_instance = vkb_inst.instance;
	// store the debug messenger
	_debug_messenger = vkb_inst.debug_messenger;

	// get the surface of the window we opened with SDL
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	// use vkbootstrap to select a gpu.
	// we want a gpu that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice{ selector
		.set_minimum_version(1, 1)
		.set_surface(_surface)
		.select()
		.value() };

	// create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	vkb::Device vkbDevice{ deviceBuilder.build().value() };

	// get the VKDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

	// use vkbootstrap to get a graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_mainDeletionQueue.push_function([=]() {
		vmaDestroyAllocator(_allocator);
	});

	vkGetPhysicalDeviceProperties(_chosenGPU, &_gpuProperties);
}

bool VulkanEngine::load_shader_module(const std::string& filePath, VkShaderModule* outShaderModule)
{
	// std::ios::ate puts cursor at end of file upon opening
	std::ifstream file{ filePath, std::ios::ate | std::ios::binary };

	if (!file.is_open()) {
		return false;
	}

	// find size of file in bytes by position of cursor
	size_t fileSize{ (size_t)file.tellg() };

	// spirv expects the buffer to be on uint32, so make sure to
	// reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	// put file cursor at beginning
	file.seekg(0);

	// load the entire file into the buffer
	file.read((char*)buffer.data(), fileSize);

	file.close();

	// create a new shader module using the buffer we loaded
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;
	//codeSize has to be in bytes, so multiply the size by sizeof(uint32_t)
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return false;
	}
	*outShaderModule = shaderModule;
	return true;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func{ (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT") };
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanEngine::cleanup()
{
	if (_isInitialized) {
		//for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		//	vkWaitForFences(_device, 1, &_frames[i]._renderFence, true, 1'000'000'000);
		//}

		vkQueueWaitIdle(_graphicsQueue);

		_mainDeletionQueue.flush();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyDevice(_device, nullptr);
		DestroyDebugUtilsMessengerEXT(_instance, _debug_messenger, nullptr);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{
	uint32_t msStartTime{ SDL_GetTicks() };

	// wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1'000'000'000));
	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

	// request image from the swapchain, one second timeout. This is also where vsync happens according to vkguide, but for me it happens at present
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1'000'000'000, get_current_frame()._presentSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

	// now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again
	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo{};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;
	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(get_current_frame()._mainCommandBuffer, &cmdBeginInfo));

	// make a clear-color from the frame number. This will flash with a 120 * pi frame period.
	VkClearValue clearValue{};
	float flash = std::abs(std::sin(_frameNumber / 120.0f));
	clearValue.color = { {0.0f, 0.0f, flash, 1.0f} };

	VkClearValue depthClear{};
	depthClear.depthStencil.depth = 1.0f;

	std::array<VkClearValue, 2> clearValues{ clearValue, depthClear };

	// start the main renderpass. we will use the clear color from above,
	// and the framebuffer corresponding to the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo{};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpInfo.pNext = nullptr;
	rpInfo.renderPass = _renderPass;
	rpInfo.renderArea.offset.x = 0;
	rpInfo.renderArea.offset.y = 0;
	rpInfo.renderArea.extent = _windowExtent;
	rpInfo.framebuffer = _framebuffers[swapchainImageIndex];
	rpInfo.clearValueCount = clearValues.size();
	rpInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(get_current_frame()._mainCommandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	draw_objects(get_current_frame()._mainCommandBuffer, _renderables);

	vkCmdEndRenderPass(get_current_frame()._mainCommandBuffer);
	VK_CHECK(vkEndCommandBuffer(get_current_frame()._mainCommandBuffer));

	// we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain
	// is ready. we will signal the _renderSemaphore, to signal that rendering has finished

	VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;
	submit.pWaitDstStageMask = &waitStage;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &get_current_frame()._presentSemaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &get_current_frame()._renderSemaphore;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &get_current_frame()._mainCommandBuffer;

	// submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	// this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that, as it's necessary that
	// drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
	presentInfo.pImageIndices = &swapchainImageIndex;

	uint32_t msEndTime{ SDL_GetTicks() };
	_msDelta = msEndTime - msStartTime;

	// This is what actually blocks for vsync at least on my system...
	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	++_frameNumber;
}

// does not enqueue deletion of buffer, this is caller's responsibility!
AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	// allocate vertex buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = memoryUsage;
	AllocatedBuffer newBuffer;
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo,
		&newBuffer._buffer,
		&newBuffer._allocation,
		nullptr));

	return newBuffer;
}

Material* VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	Material mat{};
	mat.pipeline = pipeline;
	mat.pipelineLayout = layout;
	_materials[name] = mat;
	return &_materials[name];
}

Material* VulkanEngine::get_material(const std::string& name)
{
	// search for the material and return nullptr if not found
	auto it{ _materials.find(name) };
	if (it == _materials.end()) {
		return nullptr;
	} else {
		return &(it->second);
	}
}

Mesh* VulkanEngine::get_mesh(const std::string& name)
{
	auto it{ _meshes.find(name) };
	if (it == _meshes.end()) {
		return nullptr;
	} else {
		return &(it->second);
	}
}

FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, const std::multiset<RenderObject>& renderables)
{
	// negate _camPos since remeber we're actually translating everything in the scene, not the 'camera'
	glm::mat4 view{ glm::translate(glm::mat4(1.0f), -_camPos) };
	glm::mat4 projection{ glm::perspective(glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f) };
	projection[1][1] *= -1;

	// fill a GPU camera data struct
	GPUCameraData camData{};
	camData.projection = projection;
	camData.view = view;
	camData.viewProj = projection * view;

	// copy camera data to camera buffer
	void* data;
	vmaMapMemory(_allocator, get_current_frame().cameraBuffer._allocation, &data);
	std::memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(_allocator, get_current_frame().cameraBuffer._allocation);

	// copy scene data to scene buffer
	float framed{ _frameNumber / 120.0f };

	_sceneParameters.ambientColor = { sin(framed), 0, cos(framed), 1 };
	char* sceneData;
	vmaMapMemory(_allocator, _sceneParameterBuffer._allocation, (void**)&sceneData);
	int frameIndex{ _frameNumber % FRAME_OVERLAP };
	sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
	std::memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator, _sceneParameterBuffer._allocation);

	// write all the objects' matrices into the SSBO
	void* objectData;
	vmaMapMemory(_allocator, get_current_frame().objectBuffer._allocation, &objectData);
	GPUObjectData* objectSSBO{ (GPUObjectData*)objectData };
	uint32_t idx{ 0 };
	for (const RenderObject& object : renderables) {
		objectSSBO[idx].modelMatrix = object.transformMatrix;
		++idx;
	}
	vmaUnmapMemory(_allocator, get_current_frame().objectBuffer._allocation);

	Mesh* lastMesh{ nullptr };
	Material* lastMaterial{ nullptr };

	uint32_t pipelineBinds{ 0 };
	uint32_t vertexBufferBinds{ 0 };

	idx = 0;
	for (const RenderObject& object : renderables) {
		// only bind the pipeline if it doesn't match with the already bound one
		if (object.material != lastMaterial) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;

			// camera data descriptor
			uint32_t uniformOffset{ static_cast<uint32_t>(pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex) };
			// we probably bind descriptor set here since it depends on the pipelinelayout
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &get_current_frame().globalDescriptor, 1, &uniformOffset);
			
			// object data descriptor
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &get_current_frame().objectDescriptor, 0, nullptr);

			if (object.material->textureSet != VK_NULL_HANDLE) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 2, 1, &object.material->textureSet, 0, nullptr);
			}

			++pipelineBinds;
		}

		//glm::mat4 model{ object.transformMatrix };
		//// final render matrix that we are calculating on the CPU
		//glm::mat4 mesh_matrix{ projection * view * model };

		MeshPushConstants constants{};
		constants.render_matrix = object.transformMatrix;

		vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

		// only bind the mesh if it's a different one from last bind
		if (object.mesh != lastMesh) {
			// bind the mesh vertex buffer with offset 0
			VkDeviceSize offset{ 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
			lastMesh = object.mesh;
			++vertexBufferBinds;
		}

		vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, idx);
		++idx;
	}
	//std::cout << "pipeline binds: " << pipelineBinds << "\nvertex buffer binds: " << vertexBufferBinds << "\n\n";
}

void VulkanEngine::showFPS() {
	uint32_t currentTicks{ SDL_GetTicks() };
	double currentTime{ currentTicks / 1000.0 };
	double delta{ currentTime - _lastTimeFPS };
	++_nbFrames;

	// if last fps update was more than a second ago
	if (delta >= 1.0) {
		double ms{ 1000.0 / _nbFrames };
		double fps{ _nbFrames / delta };

		std::stringstream ss;
		ss << ms << " ms [" << fps << " fps] (" << _msDelta << " ms no vsync)";

		SDL_SetWindowTitle(_window, ss.str().c_str());

		_nbFrames = 0;
		_lastTimeFPS = currentTime;
	}
}

bool VulkanEngine::process_input()
{
	double currentTime{ SDL_GetTicks() / 1000.0 };
	double delta{ currentTime - _lastTime };
	_lastTime = currentTime;

	double speed{ 3.0f };

	const Uint8* keystate{ SDL_GetKeyboardState(nullptr) };

	// continuous-response keys
	if (keystate[SDL_SCANCODE_W]) {
		_camPos.z -= speed * delta;
	}
	if (keystate[SDL_SCANCODE_A]) {
		_camPos.x -= speed * delta;
	}
	if (keystate[SDL_SCANCODE_S]) {
		_camPos.z += speed * delta;
	}
	if (keystate[SDL_SCANCODE_D]) {
		_camPos.x += speed * delta;
	}
	if (keystate[SDL_SCANCODE_E]) {
		_camPos.y += speed * delta;
	}
	if (keystate[SDL_SCANCODE_Q]) {
		_camPos.y -= speed * delta;
	}

	bool bQuit{ false };
	SDL_Event e;
	// Handle events on queue
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
		case SDL_MOUSEMOTION:
			break;
		case SDL_QUIT:
			bQuit = true;
			break;
		case SDL_KEYDOWN:
			if (e.key.keysym.sym == SDLK_f) {
				std::cout << "Thank you for pressing F\n";
			}
		}
	}
	return bQuit;
}

void VulkanEngine::run()
{
	bool bQuit{ false };

	// main loop
	while (!bQuit) {
		bQuit = process_input();
		draw();
		showFPS();
	}
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
{
	// make viewport state from our stored viewport and scissor
	// at the moment we won't support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &_viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &_scissor;

	// setup dummy color blending. We aren't using transparent objects yet
	// the blending is just "no blend", but we do write to the color attachment
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &_colorBlendAttachment;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;

	pipelineInfo.stageCount = _shaderStages.size();
	pipelineInfo.pStages = _shaderStages.data();
	pipelineInfo.pVertexInputState = &_vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &_inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &_rasterizer;
	pipelineInfo.pMultisampleState = &_multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.renderPass = pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.pDepthStencilState = &_depthStencil;

	// it's easy to get errors here so we handle it more explicitly than with VK_CHECK
	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
		std::cout << "failed to create pipeline\n";
		return VK_NULL_HANDLE;
	} else {
		return newPipeline;
	}
}

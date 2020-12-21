
#include "vk_engine.h"

#include <iostream>
#include <cmath>
#include <fstream>
#include <cstring> // memcpy
#include <array>
#include <sstream>
#include <fstream>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "SDL.h"
#include "SDL_vulkan.h"

#include "vk_types.h"
#include "vk_initializers.h"
#include "VkBootstrap.h"
#include "glm/gtx/transform.hpp"

#define VK_CHECK(x)\
	do\
	{\
		VkResult err = x;\
		if (err) {\
			std::cout << "Detected Vulkan error: " << err << '\n';\
			abort();\
		}\
	} while (0)\

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
	load_materials();
	load_meshes();
	init_scene();

	// everything went fine
	_isInitialized = true;
}

void VulkanEngine::init_scene()
{
	_camPos = glm::vec3{ 0.0f, -6.0f, -10.0f };

	RenderObject monkey{};
	monkey.mesh = get_mesh("monkey");
	monkey.material = get_material("default");
	monkey.transformMatrix = glm::translate(glm::mat4{ 1.0 }, glm::vec3(0.0, 3.0, 0.0));

	_renderables.push_back(monkey);

	for (auto x{ -20 }; x <= 20; ++x) {
		for (auto y{ -20 }; y <= 20; ++y) {
			RenderObject tri{};
			glm::mat4 scale;
			if (x % 2) {
				tri.mesh = get_mesh("monkey");
				scale = glm::scale(glm::mat4{1.0}, glm::vec3(0.2, 0.2, 0.2));
			} else {
				tri.mesh = get_mesh("penguin");
				scale = glm::scale(glm::mat4{1.0}, glm::vec3(1.0, 1.0, 1.0));
			}
			tri.material = get_material("default");
			glm::mat4 translation{ glm::translate(glm::mat4{1.0}, glm::vec3(x, 0, y)) };
			tri.transformMatrix = translation * scale;

			_renderables.push_back(tri);
		}
	}
}

// upload to GPU
void VulkanEngine::upload_mesh(Mesh& mesh)
{
	// allocate vertex buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	// this is the total size, in bytes, of the buffer we are allocating
	bufferInfo.size = mesh._vertices.size() * sizeof(Vertex);
	// this buffer is going to be used as a vertex buffer
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	// let the VMA library know that htis data should be writeable by CPU,
	// but also readable by GPU
	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo,
		&mesh._vertexBuffer._buffer,
		&mesh._vertexBuffer._allocation,
		nullptr));

	// add the destruction of the triangle mesh buffer to the deletion queue
	_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer, mesh._vertexBuffer._allocation);
	});

	void* data;
	vmaMapMemory(_allocator, mesh._vertexBuffer._allocation, &data);
	std::memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(_allocator, mesh._vertexBuffer._allocation);
}

// load_mesh("monkey", "../../assets/monkey_smooth.obj")

void VulkanEngine::load_meshes()
{
	std::string name, path;
	std::string prefix{ "../../assets/" };
	std::ifstream file{ "../../assets/load_meshes.txt" };
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
	std::ifstream file{ "../../shaders/load_materials.txt" };
	while (file >> name >> vertPath >> fragPath) {
		init_pipeline(name, prefix + vertPath, prefix + fragPath);
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

	VkPipelineLayoutCreateInfo mesh_pipeline_layout_info{ vkinit::pipeline_layout_create_info() };
	VkPushConstantRange push_constant{};
	// this push constant range starts at the beginning
	push_constant.offset = 0;
	push_constant.size = sizeof(MeshPushConstants);
	// this push constant range is accessible only in the vertex shader
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
	mesh_pipeline_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout layout;
	VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &layout));

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

	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _renderFence, nullptr);
	});

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));
	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));

	_mainDeletionQueue.push_function([=]() {
		vkDestroySemaphore(_device, _presentSemaphore, nullptr);
		vkDestroySemaphore(_device, _renderSemaphore, nullptr);
	});
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
	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

	// allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo{ vkinit::command_buffer_allocate_info(_commandPool, 1) };
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(_device, _commandPool, nullptr);
	});
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

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func{ (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT") };
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanEngine::cleanup()
{
	if (_isInitialized) {
		vkWaitForFences(_device, 1, &_renderFence, true, 1'000'000'000);

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
	// wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &_renderFence, true, 1'000'000'000));
	VK_CHECK(vkResetFences(_device, 1, &_renderFence));

	// request image from the swapchain, one second timeout. This is also where vsync happens
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1'000'000'000, _presentSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

	// now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again
	VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo{};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;
	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(_mainCommandBuffer, &cmdBeginInfo));

	// make a clear-color from the frame number. This will flash with a 120 frame period.
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

	vkCmdBeginRenderPass(_mainCommandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	draw_objects(_mainCommandBuffer, _renderables.data(), _renderables.size());

	vkCmdEndRenderPass(_mainCommandBuffer);
	VK_CHECK(vkEndCommandBuffer(_mainCommandBuffer));

	// we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	// we will signal the _renderSemaphore, to signal that rendering has finished

	VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;
	submit.pWaitDstStageMask = &waitStage;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &_presentSemaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_renderSemaphore;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &_mainCommandBuffer;

	// submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFence));

	// this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that, as it's necessary that
	// drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &_renderSemaphore;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	++_frameNumber;
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

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject* first, int count)
{
	glm::mat4 view{ glm::translate(glm::mat4(1.0f), _camPos) };
	glm::mat4 projection{ glm::perspective(glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f) };
	projection[1][1] *= -1;

	Mesh* lastMesh{ nullptr };
	Material* lastMaterial{ nullptr };

	uint32_t pipelineBinds{ 0 };
	uint32_t vertexBufferBinds{ 0 };

	for (auto i{ 0 }; i < count; ++i) {
		RenderObject& object{ first[i] };

		// only bind the pipeline if it doesn't match with the already bound one
		if (object.material != lastMaterial) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;
			++pipelineBinds;
		}

		glm::mat4 model{ object.transformMatrix };
		// final render matrix that we are calculating on the CPU
		glm::mat4 mesh_matrix{ projection * view * model };

		MeshPushConstants constants{};
		constants.render_matrix = mesh_matrix;

		vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

		// only bind the mesh if it's a different one from last bind
		if (object.mesh != lastMesh) {
			// bind the mesh vertex buffer with offset 0
			VkDeviceSize offset{ 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
			lastMesh = object.mesh;
			++vertexBufferBinds;
		}
		vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, 0);
	}
		//std::cout << "pipeline binds: " << pipelineBinds << "\nvertex buffer binds: " << vertexBufferBinds << "\n";
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
		ss << ms << " ms   [" << fps << " fps]";

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
		_camPos.z += speed * delta;
	}
	if (keystate[SDL_SCANCODE_A]) {
		_camPos.x += speed * delta;
	}
	if (keystate[SDL_SCANCODE_S]) {
		_camPos.z -= speed * delta;
	}
	if (keystate[SDL_SCANCODE_D]) {
		_camPos.x -= speed * delta;
	}
	if (keystate[SDL_SCANCODE_E]) {
		_camPos.y -= speed * delta;
	}
	if (keystate[SDL_SCANCODE_Q]) {
		_camPos.y += speed * delta;
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

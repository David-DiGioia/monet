
#include "vk_engine.h"

#include <iostream>
#include <cmath>
#include <fstream>
#include <cstring> // memcpy
#include <array>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <filesystem>

#include "SDL.h"
#include "SDL_vulkan.h"
#include "vk_types.h"
#include "vk_initializers.h"
#include "VkBootstrap.h"
#include "glm/gtx/transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "vk_textures.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include "render_to_texture.h"
#include "../tracy/Tracy.hpp"		// CPU profiling
#include "../tracy/TracyVulkan.hpp"	// GPU profiling
#include "SDL_mixer.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

bool RenderObject::operator<(const RenderObject& other) const
{
	if (material->pipeline == other.material->pipeline) {
		return mesh->_vertexBuffer._buffer < other.mesh->_vertexBuffer._buffer;
	} else {
		return material->pipeline < other.material->pipeline;
	}
}

template<typename Mat, typename Stream>
void printMat(const Mat& mat, Stream& out) {
	for (auto row{ 0 }; row < mat.length(); ++row) {
		for (auto col{ 0 }; col < mat.length(); ++col) {
			out << mat[col][row] << '\t';
		}
		out << '\n';
	}
	out << '\n';
}

template<typename Mat>
void printMat(const Mat& mat) {
	printMat(mat, std::cout);
}

Transform::Transform()
	: pos{ 0.0 }
	, scale{ 1.0 }
	, rot{ 1.0 }
{}

Transform::Transform(const PxTransform& pxt)
	: pos{ pxt.p.x, pxt.p.y, pxt.p.z }
	, scale{ 1.0 }
{
	glm::quat q{};
	q.x = pxt.q.x;
	q.y = pxt.q.y;
	q.z = pxt.q.z;
	q.w = pxt.q.w;
	rot = glm::toMat4(q);
}

glm::mat4 Transform::mat4()
{
	return glm::translate(glm::mat4{ 1.0 }, pos) * rot * glm::scale(glm::mat4{ 1.0 }, scale);
}

physx::PxTransform Transform::to_physx()
{
	PxTransform result{};
	result.p.x = pos.x;
	result.p.y = pos.y;
	result.p.z = pos.z;

	glm::quat q{ glm::quat_cast(rot) };
	result.q.x = q.x;
	result.q.y = q.y;
	result.q.z = q.z;
	result.q.w = q.w;

	return result;
}

void GameObject::setRenderObject(const RenderObject* ro)
{
	_renderObject = ro;
}

physx::PxRigidActor* GameObject::getPhysicsObject()
{
	return _physicsObject;
}

void GameObject::setPhysicsObject(physx::PxRigidActor* body)
{
	_physicsObject = body;
}

void GameObject::updateRenderMatrix()
{
	_renderObject->transformMatrix = _transform.mat4();
}

Transform GameObject::getTransform()
{
	return _transform;
}

glm::vec3 GameObject::getPos()
{
	return _transform.pos;
}

glm::vec3 GameObject::getScale()
{
	return _transform.scale;
}

glm::mat4 GameObject::getRot()
{
	return _transform.rot;
}

void GameObject::setTransform(Transform transform)
{
	_transform = transform;
	updateRenderMatrix();
}

void GameObject::setPos(glm::vec3 pos)
{
	_transform.pos = pos;
	updateRenderMatrix();
}

void GameObject::setScale(glm::vec3 scale)
{
	_transform.scale = scale;
	updateRenderMatrix();
}

void GameObject::setRot(glm::mat4 rot)
{
	_transform.rot = rot;
	updateRenderMatrix();
}

void GameObject::addForce(glm::vec3 force)
{
	_physicsObject->is<PxRigidDynamic>()->addForce(physx::PxVec3{ force.x, force.y, force.z });
}

void GameObject::setVelocity(glm::vec3 velocity)
{
	PxVec3 v{};
	v.x = velocity.x;
	v.y = velocity.y;
	v.z = velocity.z;
	_physicsObject->is<PxRigidDynamic>()->setLinearVelocity(v);
}

void GameObject::setMass(float mass)
{
	_physicsObject->is<PxRigidDynamic>()->setMass(mass);
}

float GameObject::getMass()
{
	return _physicsObject->is<PxRigidDynamic>()->getMass();
}

void VulkanEngine::init(Application* app)
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		std::cout << "Error: " << Mix_GetError() << "\n";
	}

	//// general rule is > 10 sec is music, otherwise chunk
	//Mix_Music* bgm{ Mix_LoadMUS("../../assets/audio/charlie.mp3") };
	//Mix_Chunk* soundEffect{ Mix_LoadWAV("../../assets/audio/rode_mic.wav") };

	//Mix_PlayMusic(bgm, -1);

	//_mainDeletionQueue.push_function([=]() {
	//	Mix_FreeChunk(soundEffect);
	//	Mix_FreeMusic(bgm);
	//});

	//SDL_SetRelativeMouseMode((SDL_bool)_camMouseControls);

	SDL_WindowFlags window_flags{ (SDL_WindowFlags)(SDL_WINDOW_VULKAN) };

	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);

	_physicsEngine.initPhysics();

	_app = app;
	init_vulkan();
	init_swapchain();
	init_commands();
	init_tracy();
	init_default_renderpass();
	init_framebuffers();
	init_sync_structures();
	load_meshes();
	init_descriptor_pool();
	init_object_buffers();
	init_shadow_pass();
	init_descriptors(); // descriptors are needed at pipeline create, so before materials
	load_materials();
	init_scene();
	init_imgui();
	init_gui_data();

	// everything went fine
	_isInitialized = true;
}

void VulkanEngine::set_camera_transform(Transform transform)
{
	_camTransform = transform;
}

void VulkanEngine::set_scene_lights(const std::vector<Light>& lights)
{
	uint32_t numLights = (uint32_t)lights.size();
	if (numLights > MAX_NUM_TOTAL_LIGHTS) {
		std::cout << "Error: numLights > MAX_NUM_TOTAL_LIGHTS\n";
	}

	_sceneParameters.numLights = numLights;

	for (auto i{ 0 }; i < numLights; ++i) {
		_sceneParameters.lights[i] = lights[i];
	}
}

void VulkanEngine::init_tracy()
{
	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		VkCommandBufferAllocateInfo cmdAllocInfo{ vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1) };

		VkCommandBuffer cmd;
		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &cmd));

		_frames[i]._tracyContext = TracyVkContext(_chosenGPU, _device, _graphicsQueue, cmd);

		_mainDeletionQueue.push_function([=]() {
			TracyVkDestroy(_frames[i]._tracyContext);
		});
	}


	vkResetCommandPool(_device, _frames[0]._commandPool, 0);
}

void VulkanEngine::init_gui_data()
{
	_guiData.light0.position = { 1.0, 3.9, 3.3, 1.0 };
	_guiData.light0.color = { 0.0, 0.0, 0.0, 67.0 };

	_guiData.light1.position = { 7.5, 10.0, 0.0, 1.0 };
	_guiData.light1.color = { 0.0, 0.0, 0.0, 47.0 };

	_guiData.bedAngle = 0.0f;

	_guiData.roughness_mult = 1.0f;
}

void VulkanEngine::init_imgui()
{
	// 1: create a descriptor pool for IMGUI
	// the size of the pool is very oversized, but it's copied from imgui demo itself.
	VkDescriptorPoolSize pool_sizes[]{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(_window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosenGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = (uint32_t)_swapchainImages.size();
	init_info.ImageCount = (uint32_t)_swapchainImages.size();;

	ImGui_ImplVulkan_Init(&init_info, _renderPass);

	// execute a GPU command to upload imgui font textures
	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
	});

	// clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	// enqueue destruction of the imgui created structures
	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
	});
}

void VulkanEngine::init_scene()
{
	RenderObject cube{};
	cube.mesh = get_mesh("cube");
	cube.material = get_material("testCubemapMat");
	cube.transformMatrix = glm::mat4{ 1.0 };
	cube.castShadow = false;
	_renderables.insert(cube);

	_app->init(*this);
}

const RenderObject* VulkanEngine::create_render_object(const std::string& meshName, const std::string& matName, bool castShadow)
{
	RenderObject object{};
	object.mesh = get_mesh(meshName);
	object.material = get_material(matName);
	object.transformMatrix = glm::mat4(1.0);
	object.castShadow = castShadow;
	_renderables.insert(object);

	return &(*_renderables.find(object));
}

const RenderObject* VulkanEngine::create_render_object(const std::string& name)
{
	return create_render_object(name, name);
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

	vkWaitForFences(_device, 1, &_uploadContext._uploadFence, VK_TRUE, 99999999999);
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

void VulkanEngine::load_meshes()
{
	namespace fs = std::filesystem;
	std::string modelsPath{ "../../assets/models/" };

	for (const auto& modelDir : fs::directory_iterator(modelsPath)) {
		if (modelDir.is_directory()) {

			for (const auto& file : fs::directory_iterator(modelDir)) {
				std::string filepath{ file.path().generic_string() };

				if (filepath.size() >= 4 && filepath.substr(filepath.size() - 4) == ".obj") {
					std::string name{ modelDir.path().filename().generic_string() };
					std::cout << "Loading mesh '" << name << "'\n";
					load_mesh(name, filepath);
				}
			}
		}
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

void VulkanEngine::load_texture(const std::string& path, VkFormat format)
{
	Texture texture;
	uint32_t mipLevels;
	std::string prefix{ "../../assets/models/" };

	if (!vkutil::load_image_from_file(*this, (prefix + path).c_str(), texture.image, &mipLevels, format)) {
		std::cout << "Failed to load texture: " << path << "\n";
	}

	VkImageViewCreateInfo imageInfo{ vkinit::imageview_create_info(format, texture.image._image, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels) };
	vkCreateImageView(_device, &imageInfo, nullptr, &texture.imageView);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, texture.imageView, nullptr);
	});

	texture.mipLevels = mipLevels;

	_loadedTextures[path] = texture;
}

void VulkanEngine::load_materials()
{
	std::string prefix{ "../../shaders/" };
	std::string loadFile{ "_load_materials.txt" };
	std::ifstream file{ prefix + loadFile };
	std::string line;

	std::unordered_map<std::string, VkDescriptorType> stringToType;
	stringToType["COMBINED_IMAGE_SAMPLER"] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	std::unordered_map<std::string, VkShaderStageFlagBits> stringToStage;
	stringToStage["VERTEX"] = VK_SHADER_STAGE_VERTEX_BIT;
	stringToStage["FRAGMENT"] = VK_SHADER_STAGE_FRAGMENT_BIT;
	std::unordered_map<std::string, VkFormat> stringToFormat;
	stringToFormat["R8G8B8A8_SRGB"] = VK_FORMAT_R8G8B8A8_SRGB;
	stringToFormat["R8G8B8A8_UNORM"] = VK_FORMAT_R8G8B8A8_UNORM;
	stringToFormat["R32G32B32A32_SFLOAT"] = VK_FORMAT_R32G32B32A32_SFLOAT;


	while (file) {

		MaterialCreateInfo info{};
		std::vector<std::string> bindingPaths;
		uint32_t bindingIdx{ 0 };

		// cubemap variables
		std::string cubemapMaterial, cubemapTexName, useMipmap, isCubemap, cubeVertPath, cubeFragPath;
		uint32_t cubemapRes{};

		bool blockComment{ false };

		while (std::getline(file, line)) {

			if (line == "/*") {
				blockComment = true;
				continue;
			} else if (line == "*/") {
				blockComment = false;
				continue;
			}

			// skip comments and blank lines
			if (blockComment || line.size() < 2 || (line.substr(0, 2) == "//")) {
				continue;
			}

			std::stringstream ss{ line };

			std::string field;
			ss >> field;

			if (field == "name:") {
				ss >> info.name;
				if (info.name != "") {
					std::cout << "Loading material '" << info.name << "'\n";
				}
			} else if (field == "vert:") {
				std::string shader;
				ss >> shader;
				info.vertPath = prefix + shader;
			} else if (field == "frag:") {
				std::string shader;
				ss >> shader;
				info.fragPath = prefix + shader;
			} else if (field == "render_to_texture:") {
				std::string cubemapResStr;
				file >> cubemapMaterial >> cubemapTexName >> cubemapResStr >> useMipmap >> isCubemap >> cubeVertPath >> cubeFragPath;
				std::cout << "Rendering to texture '" << cubemapTexName << "'\n";
				cubemapRes = static_cast<uint32_t>(std::stoul(cubemapResStr));
			} else if (field == "bind:") {
				VkDescriptorSetLayoutBinding binding{};
				binding.descriptorCount = 1;
				binding.binding = bindingIdx;
				++bindingIdx;

				std::string lineBind;

				while (std::getline(file, lineBind)) {
					std::stringstream ssBind{ lineBind };
					std::string fieldBind;
					ssBind >> fieldBind;

					if (fieldBind == "type:") {
						std::string type;
						ssBind >> type;
						binding.descriptorType = stringToType[type];
					} else if (fieldBind == "stage:") {
						std::string stage;
						VkShaderStageFlags flags{};

						while (ssBind >> stage) {
							flags |= stringToStage[stage];
						}

						binding.stageFlags = flags;
					} else if (fieldBind == "path:") {
						std::string path;
						ssBind >> path;
						bindingPaths.push_back(path);
					} else if (fieldBind == "format:") {
						std::string format;
						ssBind >> format;
						// If this texture is not already loaded (via cubemap), load it
						if (_loadedTextures.find(bindingPaths.back()) == _loadedTextures.end()) {
							load_texture(bindingPaths.back(), stringToFormat[format]);
						}
						break;
					}
				}
				info.bindings.push_back(binding);

			} else if (field == "END") {
				break;
			}
		}
		if (info.name != "") {
			info.bindingTextures = textures_from_binding_paths(bindingPaths);
			init_pipeline(info, prefix);
		}

		if (cubemapTexName != "") {
			VkExtent2D textureRes{};
			textureRes.width = cubemapRes;
			textureRes.height = cubemapRes;
			bool useMip{ useMipmap == "true" };
			bool isCube{ isCubemap == "true" };

			Material* cubemapMat{ get_material(cubemapMaterial) };
			Texture cubemap{ render_to_texture(*this, cubemapMat->textureSet, textureRes, useMip, isCube, cubeVertPath, cubeFragPath) };

			_loadedTextures[cubemapTexName] = cubemap;
		}
	}

	file.close();
}

void VulkanEngine::init_pipeline(const MaterialCreateInfo& info, const std::string& prefix)
{
	VkShaderModule vertShader;
	if (!load_shader_module(info.vertPath, &vertShader)) {
		std::cout << "Error when building vertex shader module: " << info.vertPath << "\n";
	}

	VkShaderModule fragShader;
	if (!load_shader_module(info.fragPath, &fragShader)) {
		std::cout << "Error when building fragment shader module: " << info.fragPath << "\n";
	}

	VkPipelineLayoutCreateInfo pipeline_layout_info{ vkinit::pipeline_layout_create_info() };
	VkPushConstantRange push_constant{};
	// this push constant range starts at the beginning
	push_constant.offset = 0;
	push_constant.size = sizeof(MeshPushConstants);
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	// create the descriptor set layout specific to the material we're making

	VkDescriptorSetLayoutCreateInfo materialSetLayoutInfo{};
	materialSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	materialSetLayoutInfo.pNext = nullptr;
	materialSetLayoutInfo.bindingCount = info.bindings.size();
	materialSetLayoutInfo.pBindings = info.bindings.data();

	VkDescriptorSetLayout materialSetLayout;
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &materialSetLayoutInfo, nullptr, &materialSetLayout));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(_device, materialSetLayout, nullptr);
	});

	std::array<VkDescriptorSetLayout, 3> setLayouts{ _globalSetLayout, _objectSetLayout, materialSetLayout };

	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pPushConstantRanges = &push_constant;
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
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertShader)
	);

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader)
	);

	VkPipeline pipeline{ pipelineBuilder.build_pipeline(_device, _renderPass) };

	create_material(info, pipeline, layout, materialSetLayout);

	vkDestroyShaderModule(_device, vertShader, nullptr);
	vkDestroyShaderModule(_device, fragShader, nullptr);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipeline(_device, pipeline, nullptr);
		vkDestroyPipelineLayout(_device, layout, nullptr);
	});
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

void VulkanEngine::init_descriptor_pool() {
	std::vector<VkDescriptorPoolSize> sizes{
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 }
	};

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = 100;
	pool_info.poolSizeCount = (uint32_t)sizes.size();
	pool_info.pPoolSizes = sizes.data();

	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptorPool));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
	});
}

void VulkanEngine::init_object_buffers() {
	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		_frames[i].objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(_allocator, _frames[i].objectBuffer._buffer, _frames[i].objectBuffer._allocation);
		});
	}
}

void VulkanEngine::init_descriptors()
{
	// cameraBind needs to be accessed from fragment shader to get camPos
	VkDescriptorSetLayoutBinding cameraBind{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };
	VkDescriptorSetLayoutBinding sceneBind{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1) };
	VkDescriptorSetLayoutBinding shadowMapBind{ vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2) };

	std::array<VkDescriptorSetLayoutBinding, 3> bindings{ cameraBind, sceneBind, shadowMapBind };

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

	VK_CHECK(vkCreateDescriptorSetLayout(_device, &setInfo, nullptr, &_globalSetLayout));
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &setInfo2, nullptr, &_objectSetLayout));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
	});

	const size_t sceneParamBufferSize{ FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData)) };
	_sceneParameterBuffer = create_buffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(_allocator, _sceneParameterBuffer._buffer, _sceneParameterBuffer._allocation);
	});

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {

		_frames[i].cameraBuffer = create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		_mainDeletionQueue.push_function([=]() {
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

		VkDescriptorImageInfo shadowMapInfo{};
		// offscreen renderpass that generates shadow map transitions it to this layout once it's finished
		shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadowMapInfo.imageView = _frames[i]._shadow.depth.imageView;
		shadowMapInfo.sampler = _frames[i]._shadow.depthSampler;

		VkDescriptorBufferInfo objectInfo{};
		objectInfo.buffer = _frames[i].objectBuffer._buffer;
		objectInfo.offset = 0;
		objectInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;

		VkWriteDescriptorSet cameraWrite{ vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _frames[i].globalDescriptor, &cameraInfo, 0) };
		VkWriteDescriptorSet sceneWrite{ vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _frames[i].globalDescriptor, &sceneInfo, 1) };
		VkWriteDescriptorSet shadowMapWrite{ vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _frames[i].globalDescriptor, &shadowMapInfo, 2) };
		VkWriteDescriptorSet objectWrite{ vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _frames[i].objectDescriptor, &objectInfo, 0) };
		std::array<VkWriteDescriptorSet, 4> setWrites{ cameraWrite, sceneWrite, shadowMapWrite, objectWrite };
		vkUpdateDescriptorSets(_device, setWrites.size(), setWrites.data(), 0, nullptr);
	}
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
	// the attachment will be transitioned to this layout by the implementation
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

	// added for shadow map:
	std::array<VkSubpassDependency, 2> dependencies{};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	// early fragment since that's where depth attachment (not shadowmap) is load_op_clear'd
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	// depth_write for load_op_clear for depth pass (not shadowmap)
	dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

	render_pass_info.dependencyCount = dependencies.size();
	render_pass_info.pDependencies = dependencies.data();

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
		.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
		//.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
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
	VkImageCreateInfo dimg_info{ vkinit::image_create_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent, 1) };

	// for the depth image, we want to allocate it from GPU local memory
	VmaAllocationCreateInfo dimg_allocinfo{};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dimg_allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr);
	// build an imageView for the depth image to use for rendering
	VkImageViewCreateInfo dview_info{ vkinit::imageview_create_info(_depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT, 1) };

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

	//VkPhysicalDeviceFeatures features{};
	//features.shaderFloat64 = VK_TRUE;

	// use vkbootstrap to select a gpu.
	// we want a gpu that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice{ selector
		.set_minimum_version(1, 1)
		.set_surface(_surface)
		//.set_required_features(features)
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

		Mix_Quit();

		SDL_DestroyWindow(_window);
	}
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

Material* VulkanEngine::create_material(const MaterialCreateInfo& info, VkPipeline pipeline, VkPipelineLayout layout, VkDescriptorSetLayout materialSetLayout)
{
	Material mat{};
	mat.pipeline = pipeline;
	mat.pipelineLayout = layout;

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = _descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &materialSetLayout;

	VK_CHECK(vkAllocateDescriptorSets(_device, &allocInfo, &mat.textureSet));

	for (auto i{ 0 }; i < info.bindings.size(); ++i) {
		const Texture& texture{ info.bindingTextures[i] };
		VkSamplerCreateInfo samplerInfo{ vkinit::sampler_create_info(VK_FILTER_LINEAR, texture.mipLevels, VK_SAMPLER_ADDRESS_MODE_REPEAT) };
		VkSampler sampler;
		vkCreateSampler(_device, &samplerInfo, nullptr, &sampler);

		_mainDeletionQueue.push_function([=]() {
			vkDestroySampler(_device, sampler, nullptr);
		});

		VkDescriptorImageInfo imageBufferInfo{};
		imageBufferInfo.sampler = sampler;
		imageBufferInfo.imageView = texture.imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet textureWrite{ vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mat.textureSet, &imageBufferInfo, i) };

		vkUpdateDescriptorSets(_device, 1, &textureWrite, 0, nullptr);
	}

	_materials[info.name] = mat;
	return &_materials[info.name];
}

std::vector<Texture> VulkanEngine::textures_from_binding_paths(const std::vector<std::string>& bindingPaths)
{
	std::vector<Texture> result;
	result.reserve(bindingPaths.size());

	for (const std::string& bindingPath : bindingPaths) {
		result.push_back(_loadedTextures[bindingPath]);
	}

	return result;
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
		std::cout << " Could not find mesh '" << name << "' Did you remember to export the .obj file?\n";
		return nullptr;
	} else {
		return &(it->second);
	}
}

FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
}

void VulkanEngine::init_shadow_pass()
{
	_shadowGlobal._width = SHADOWMAP_DIM;
	_shadowGlobal._height = SHADOWMAP_DIM;

	prepareShadowMapRenderpass(*this, &_shadowGlobal._renderPass);
	std::array<VkDescriptorSetLayout, 2> setLayouts{};
	setupDescriptorSetLayouts(*this, setLayouts, &_shadowGlobal._shadowPipelineLayout);
	initShadowPipeline(*this, _shadowGlobal._renderPass, _shadowGlobal._shadowPipelineLayout, &_shadowGlobal._shadowPipeline);

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		ShadowFrameResources& shadowFrame{ _frames[i % FRAME_OVERLAP]._shadow };

		prepareShadowMapFramebuffer(*this, _shadowGlobal, &shadowFrame);

		shadowFrame._shadowLightBuffer = create_buffer(sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(_allocator, shadowFrame._shadowLightBuffer._buffer, shadowFrame._shadowLightBuffer._allocation);
		});

		setupDescriptorSets(*this, shadowFrame, _frames[i].objectBuffer._buffer, setLayouts);
	}
}

// First render pass: Generate shadow map by rendering the scene from light's POV
void VulkanEngine::shadow_pass(VkCommandBuffer& cmd)
{
	VkClearValue clearValue{};
	clearValue.color = { {1.00, 0.00, 0.00, 1.0} };

	VkClearValue depthClear{};
	depthClear.depthStencil.depth = 1.0f;

	std::array<VkClearValue, 2> clearValues{ clearValue, depthClear };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = _shadowGlobal._renderPass;
	renderPassBeginInfo.framebuffer = get_current_frame()._shadow.frameBuffer;
	renderPassBeginInfo.renderArea.extent.width = _shadowGlobal._width;
	renderPassBeginInfo.renderArea.extent.height = _shadowGlobal._height;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.width = (float)_shadowGlobal._width;
	viewport.height = (float)_shadowGlobal._height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0;
	viewport.y = 0;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent.width = _shadowGlobal._width;
	scissor.extent.height = _shadowGlobal._height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	float near_plane{ 0.5f };
	float far_plane{ 12.0f };
	float scale{ 5.0f };
	glm::mat4 lightProjection{ glm::ortho(-scale, scale, -scale, scale, near_plane, far_plane) };
	lightProjection[1][1] *= -1;

	glm::mat4 lightView{ glm::lookAt(glm::vec3(-4.0f, 8.0f, -2.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f)) };

	_shadowGlobal._lightSpaceMatrix = lightProjection * lightView;

	void* data;
	vmaMapMemory(_allocator, get_current_frame()._shadow._shadowLightBuffer._allocation, &data);
	std::memcpy(data, &_shadowGlobal._lightSpaceMatrix, sizeof(glm::mat4));
	vmaUnmapMemory(_allocator, get_current_frame()._shadow._shadowLightBuffer._allocation);

	// Set depth bias (aka "Polygon offset")
	// Required to avoid shadow mapping artifacts
	vkCmdSetDepthBias(cmd, _shadowGlobal._depthBiasConstant, 0.0f, _shadowGlobal._depthBiasSlope);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowGlobal._shadowPipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowGlobal._shadowPipelineLayout, 0, 1, &get_current_frame()._shadow._shadowDescriptorSetLight, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowGlobal._shadowPipelineLayout, 1, 1, &get_current_frame()._shadow._shadowDescriptorSetObjects, 0, nullptr);


	Mesh* lastMesh{ nullptr };

	uint32_t idx{ 0 };
	for (const RenderObject& object : _renderables) {
		if (!object.castShadow)
			continue;

		// only bind the mesh if it's a different one from last bind
		if (object.mesh != lastMesh) {
			// bind the mesh vertex buffer with offset 0
			VkDeviceSize offset{ 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
			lastMesh = object.mesh;
		}

		vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, idx);
		++idx;
	}

	vkCmdEndRenderPass(cmd);

	//VkImageMemoryBarrier barrier{};
	//barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	//barrier.image = get_current_frame()._shadow.depth.image._image;
	//barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	//barrier.subresourceRange.baseArrayLayer = 0;
	//barrier.subresourceRange.layerCount = 1;
	//barrier.subresourceRange.levelCount = 1;
	//barrier.subresourceRange.baseMipLevel = 0;
	////barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	//barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	//barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	//barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	//barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	//vkCmdPipelineBarrier(cmd,
	//	VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
	//	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
	//	0, nullptr,
	//	0, nullptr,
	//	1, &barrier);
}

void VulkanEngine::draw()
{
	ImGui::Render();

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

	// write all the objects' matrices into the SSBO (used in both shadow pass and draw objects)
	void* objectData;
	vmaMapMemory(_allocator, get_current_frame().objectBuffer._allocation, &objectData);
	GPUObjectData* objectSSBO{ (GPUObjectData*)objectData };
	uint32_t idx{ 0 };
	for (const RenderObject& object : _renderables) {
		objectSSBO[idx].modelMatrix = object.transformMatrix;
		++idx;
	}
	vmaUnmapMemory(_allocator, get_current_frame().objectBuffer._allocation);

	shadow_pass(get_current_frame()._mainCommandBuffer);

	VkClearValue clearValue{};
	clearValue.color = { {0.0, 0.0, 0.1, 1.0} };

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

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), get_current_frame()._mainCommandBuffer);

	vkCmdEndRenderPass(get_current_frame()._mainCommandBuffer);
	TracyVkCollect(get_current_frame()._tracyContext, get_current_frame()._mainCommandBuffer);

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

	FrameMark;
	++_frameNumber;
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, const std::multiset<RenderObject>& renderables)
{
	TracyVkZone(get_current_frame()._tracyContext, cmd, "Draw objects");

	_sceneParameters.lightSpaceMatrix = _shadowGlobal._lightSpaceMatrix;
	_sceneParameters.numLights = 0;

	// Assume _camTransform and _sceneParamters lights are updated here if they need to be
	_app->update(*this, _delta);

	//glm::mat4 rotTheta{ glm::rotate(_camRotTheta, glm::vec3{ 1.0f, 0.0f, 0.0f }) };
	//glm::mat4 rotPhi{ glm::rotate(_camRotPhi, glm::vec3{ 0.0f, 1.0f, 0.0f }) };
	//_camRot = rotPhi * rotTheta;
	//glm::mat4 translate{ glm::translate(glm::mat4(1.0f), _camPos) };
	//glm::mat4 view{ translate * _camRot };
	//glm::mat4 viewOrigin{ _camRot };

	glm::mat4 view{ _camTransform.mat4() };
	glm::mat4 viewOrigin{ _camTransform.rot };

	view = glm::inverse(view);
	viewOrigin = glm::inverse(viewOrigin);

	glm::mat4 projection{ glm::perspective(glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f) };
	projection[1][1] *= -1;

	// fill a GPU camera data struct
	GPUCameraData camData{};
	camData.viewProjOrigin = projection * viewOrigin; // for skybox
	camData.projection = projection;
	camData.viewProj = projection * view;

	// copy camera data to camera buffer
	void* data;
	vmaMapMemory(_allocator, get_current_frame().cameraBuffer._allocation, &data);
	std::memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(_allocator, get_current_frame().cameraBuffer._allocation);

	// copy scene data to scene buffer
	_sceneParameters.camPos = glm::vec4(_camTransform.pos, 1.0);

	char* sceneData;
	vmaMapMemory(_allocator, _sceneParameterBuffer._allocation, (void**)&sceneData);
	int frameIndex{ _frameNumber % FRAME_OVERLAP };
	sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
	std::memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator, _sceneParameterBuffer._allocation);

	//glm::mat4 bedTranslate{ glm::translate(glm::mat4{ 1.0 }, glm::vec3(0.0, 0.0, 0.0)) };
	//glm::mat4 bedScale{ glm::scale(glm::mat4{1.0}, glm::vec3{1.0}) };
	//glm::mat4 bedRotate{ glm::rotate(_guiData.bedAngle, glm::vec3{ 0.0, 1.0, 0.0 }) };
	//_guiData.bed->transformMatrix = bedTranslate * bedScale * bedRotate;

	// we use to write object matrices to SSBO right here

	Mesh* lastMesh{ nullptr };
	Material* lastMaterial{ nullptr };

	uint32_t pipelineBinds{ 0 };
	uint32_t vertexBufferBinds{ 0 };

	uint32_t idx{ 0 };
	for (const RenderObject& object : renderables) {
		// only bind the pipeline if it doesn't match with the already bound one
		if (object.material != lastMaterial) {
			// if this material has the same descriptor set layout then the pipelines might be the same and we don't have to rebind??
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
		constants.roughness_multiplier = glm::vec4{ _guiData.roughness_mult };

		vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &constants);

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

void VulkanEngine::gui()
{
	// imgui new frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame(_window);
	ImGui::NewFrame();

	// imgui commands ---------------------------------------

	_app->gui();
}

void VulkanEngine::add_to_physics_engine_dynamic(GameObject* go, PxShape* shape, float density)
{
	// if the gameobject is null, just add the physics object to the physics engine and don't associate it with a gameobject
	if (go) {
		PxRigidDynamic* physicsObject{ _physicsEngine.addToPhysicsEngineDynamic(go->getTransform().to_physx(), shape, density) };
		go->setPhysicsObject(physicsObject);
		_physicsObjects.push_back(go);
	} else {
		_physicsEngine.addToPhysicsEngineDynamic(PxTransform{}, shape, density);
	}
}

void VulkanEngine::add_to_physics_engine_dynamic_mass(GameObject* go, PxShape* shape, float mass)
{
	if (go) {
		PxRigidDynamic* physicsObject{ _physicsEngine.addToPhysicsEngineDynamicMass(go->getTransform().to_physx(), shape, mass) };
		go->setPhysicsObject(physicsObject);
		_physicsObjects.push_back(go);
	} else {
		_physicsEngine.addToPhysicsEngineDynamicMass(PxTransform{}, shape, mass);
	}
}

void VulkanEngine::add_to_physics_engine_static(GameObject* go, PxShape* shape)
{
	if (go) {
		PxRigidStatic* physicsObject{ _physicsEngine.addToPhysicsEngineStatic(go->getTransform().to_physx(), shape) };
		go->setPhysicsObject(physicsObject);
		_physicsObjects.push_back(go);
	} else {
		_physicsEngine.addToPhysicsEngineStatic(PxTransform{}, shape);
	}
}

void VulkanEngine::set_gravity(float gravity)
{
	_physicsEngine.setGravity(gravity);
}

bool VulkanEngine::advance_physics(float delta)
{
	_physicsAccumulator += delta;
	if (_physicsAccumulator < _physicsStepSize) {
		return false;
	}

	_physicsAccumulator -= _physicsStepSize;

	_app->fixedUpdate(*this);
	_physicsEngine.stepPhysics(_physicsStepSize);
	return true;
}

void VulkanEngine::update_physics()
{
	// Advance forward simulation
	advance_physics(_delta);

	// Update gameobject transforms to match the transforms of the physics objects
	for (GameObject* go : _physicsObjects) {
		Transform t{ _physicsEngine.getActorTransform(go->getPhysicsObject()) };
		go->setTransform(t);
	}
}

PxMaterial* VulkanEngine::create_physics_material(float staticFriciton, float dynamicFriction, float restitution)
{
	return _physicsEngine.createMaterial(staticFriciton, dynamicFriction, restitution);
}

PxShape* VulkanEngine::create_physics_shape(const PxGeometry& geometry, const PxMaterial& material, bool isExclusive, PxShapeFlags shapeFlags)
{
	return _physicsEngine.createShape(geometry, material, isExclusive, shapeFlags);
}

void VulkanEngine::run()
{
	bool bQuit{ false };
	_lastTime = std::chrono::high_resolution_clock::now();

	// main loop
	while (!bQuit) {
		auto currentTime{ std::chrono::high_resolution_clock::now() };
		_delta = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - _lastTime).count() / 1000000.0f;
		_lastTime = currentTime;

		bQuit = _app->input(_delta);
		gui();
		draw();
		showFPS();
		update_physics();
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

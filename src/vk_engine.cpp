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
#include "util.h"
#include "texture_asset.h"
#include "cereal/archives/binary.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

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

physx::PxTransform Transform::toPhysx()
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

void GameObject::playAnimation(const std::string& name)
{
	_renderObject->activeAnimation = _renderObject->mesh->skel.animNameToIndex[name];
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
	if (_renderObject) {
		_renderObject->uniformBlock.transformMatrix = getGlobalMat4();
	}

	// update children render matrices
	for (GameObject* child : _children) {
		child->updateRenderMatrix();
	}
}

Transform GameObject::getTransform()
{
	return _transform;
}

glm::mat4 GameObject::getGlobalMat4()
{
	GameObject* go{ this };
	glm::mat4 result{ _transform.mat4() };
	go = go->_parent;

	while (go) {
		result = go->getTransform().mat4() * result;
		go = go->_parent;
	}

	return result;
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

void GameObject::setForceStepInterpolation(bool x)
{
	_renderObject->mesh->skel.forceStepInterpolation = x;
}

void GameObject::setParent(GameObject* parent)
{
	_parent = parent;
	parent->_children.push_back(this);
}

void GameObject::addForce(glm::vec3 force)
{
	_physicsObject->is<PxRigidDynamic>()->addForce(physx::PxVec3{ force.x, force.y, force.z });
}

void GameObject::addTorque(glm::vec3 torque)
{
	_physicsObject->is<PxRigidDynamic>()->addTorque(physx::PxVec3{ torque.x, torque.y, torque.z });
}

void GameObject::setVelocity(glm::vec3 velocity)
{
	PxVec3 v{};
	v.x = velocity.x;
	v.y = velocity.y;
	v.z = velocity.z;
	_physicsObject->is<PxRigidDynamic>()->setLinearVelocity(v);
}

// This should be called on dynamic actors. See comments for setGlobalPose for static actors.
void GameObject::setPhysicsTransform(Transform transform)
{
	this->getPhysicsObject()->setGlobalPose(transform.toPhysx());
}

void GameObject::setMass(float mass)
{
	_physicsObject->is<PxRigidDynamic>()->setMass(mass);
}

float GameObject::getMass()
{
	return _physicsObject->is<PxRigidDynamic>()->getMass();
}

//int resizingEventWatcher(void* data, SDL_Event* event) {
//	if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED) {
//		SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
//		VulkanEngine* engine{ (VulkanEngine*)data };
//		if (win == engine->_window && (event->window.data1 == 0 || event->window.data1 == 0)) {
//			int width{ 0 };
//			int height{ 0 };
//
//			while (width == 0 || height == 0) {
//				SDL_GetWindowSize(engine->_window, &width, &height);
//			}
//		}
//	}
//	return 0;
//}

void VulkanEngine::init(Application* app)
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		std::cout << "Error: " << Mix_GetError() << "\n";
	}

	//// general rule is > 10 sec is music, otherwise chunk
	//Mix_Music* bgm{ Mix_LoadMUS("../../asset/assets/audio/charlie.mp3") };
	//Mix_Chunk* soundEffect{ Mix_LoadWAV("../../asset/assets/audio/rode_mic.wav") };

	//Mix_PlayMusic(bgm, -1);

	//_mainDeletionQueue.push_function([=]() {
	//	Mix_FreeChunk(soundEffect);
	//	Mix_FreeMusic(bgm);
	//});

	//SDL_SetRelativeMouseMode((SDL_bool)_camMouseControls);

	SDL_WindowFlags window_flags{ (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE) };

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
	initVulkan();
	initSwapchain(VK_NULL_HANDLE);
	initCommands();
	initTracy();
	initDefaultRenderpass();
	initFramebuffers(false);
	initSyncStructures();
	initDescriptorPool();
	initObjectBuffers();
	initShadowPass();
	initDescriptors(); // descriptors are needed at pipeline create, so before materials
	loadMeshes();
	loadMaterials();
	initScene();
	initBoundingSphere();
	initImgui();
	initGuiData();

	//SDL_AddEventWatch(resizingEventWatcher, this);

	// everything went fine
	_isInitialized = true;
}

VkSampleCountFlagBits VulkanEngine::getMaxUsableSampleCount(VkPhysicalDevice physicalDevice) {
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanEngine::setCameraTransform(Transform transform)
{
	_camTransform = transform;
}

void VulkanEngine::setSceneLights(const std::vector<Light>& lights)
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

void VulkanEngine::initTracy()
{
	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		VkCommandBufferAllocateInfo cmdAllocInfo{ vkinit::commandBufferAllocateInfo(_frames[i].commandPool, 1) };

		VkCommandBuffer cmd;
		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &cmd));

		_frames[i].tracyContext = TracyVkContext(_chosenGPU, _device, _graphicsQueue, cmd);

		_mainDeletionQueue.pushFunction([=]() {
			TracyVkDestroy(_frames[i].tracyContext);
		});
	}


	vkResetCommandPool(_device, _frames[0].commandPool, 0);
}

void VulkanEngine::initGuiData()
{
	_guiData.light0.position = { 1.0, 3.9, 3.3, 1.0 };
	_guiData.light0.color = { 0.0, 0.0, 0.0, 67.0 };

	_guiData.light1.position = { 7.5, 10.0, 0.0, 1.0 };
	_guiData.light1.color = { 0.0, 0.0, 0.0, 47.0 };

	_guiData.bedAngle = 0.0f;

	_guiData.roughness_mult = 1.0f;
}

void VulkanEngine::initImgui()
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
	init_info.ImageCount = (uint32_t)_swapchainImages.size();
	init_info.MSAASamples = _msaaSamples;

	ImGui_ImplVulkan_Init(&init_info, _renderPass);

	// execute a GPU command to upload imgui font textures
	immediateSubmit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
	});

	// clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	// enqueue destruction of the imgui created structures
	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
	});
}

void VulkanEngine::initScene()
{
	createRenderObject("cube", "testCubemapMat", false);
	_sceneParameters = GPUSceneData{}; // zero out scene parameters

	_app->init(*this);
}

const RenderObject* VulkanEngine::createRenderObject(const std::string& meshName, const std::string& matName, bool castShadow)
{
	RenderObject object{};
	object.mesh = getMesh(meshName);
	object.material = getMaterial(matName);
	object.castShadow = castShadow;
	object.uniformBlock.transformMatrix = glm::mat4(1.0f);

	return &(*_renderables.insert(object));
}

const RenderObject* VulkanEngine::createRenderObject(const std::string& name)
{
	return createRenderObject(name, name);
}

void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	// allocate the default command buffer that we will use for the instant commands
	VkCommandBufferAllocateInfo cmdAllocInfo{ vkinit::commandBufferAllocateInfo(_uploadContext._commandPool, 1) };

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

void VulkanEngine::uploadMesh(Mesh* mesh)
{
	uploadBuffer(mesh->vertices, mesh->vertexBuffer, true);
	uploadBuffer(mesh->indices, mesh->indexBuffer, false);
}

void VulkanEngine::uploadMeshSkinned(Mesh* mesh)
{
	uploadBuffer(mesh->verticesSkinned, mesh->vertexBuffer, true);
	uploadBuffer(mesh->indices, mesh->indexBuffer, false);
}

void VulkanEngine::loadSkeletalAnimation(const std::string& name, const std::string& path)
{
	std::cout << "Loading skeletal animation...\n";

	assets::SkeletalAnimationDataAsset skelAsset;

	{
		std::ifstream ifs{ path, std::ios::binary };
		cereal::BinaryInputArchive iarchive(ifs);

		iarchive(skelAsset);
	}

	SkeletalAnimationData& skel{ _meshes[name]->skel };
	skel.nodes.resize(skelAsset.nodes.size());
	skel.skins.resize(skelAsset.skins.size());
	skel.animations.resize(skelAsset.animations.size());

	// convert assets to real thing

	for (int i = 0; i < skelAsset.nodes.size(); ++i) {
		if (skelAsset.nodes[i].parentIdx > -1) {
			skel.nodes[i].parent = &skel.nodes[skelAsset.nodes[i].parentIdx];
		} else {
			skel.nodes[i].parent = nullptr;
		}

		for (int32_t childIdx : skelAsset.nodes[i].children) {
			skel.nodes[i].children.push_back(&skel.nodes[childIdx]);
		}

		skel.nodes[i].matrix = skelAsset.nodes[i].matrix;
		skel.nodes[i].cachedMatrix = glm::mat4(1.0f);
		skel.nodes[i].name = skelAsset.nodes[i].name;
		skel.nodes[i].translation = skelAsset.nodes[i].translation;
		skel.nodes[i].scale = skelAsset.nodes[i].scale;
		skel.nodes[i].rotation = skelAsset.nodes[i].rotation;
	}

	for (int i = 0; i < skelAsset.skins.size(); ++i) {
		skel.skins[i].name = skelAsset.skins[i].name;
		skel.skins[i].skeletonRoot = &skel.nodes[skelAsset.skins[i].skeletonRootIdx];
		skel.skins[i].inverseBindMatrices = skelAsset.skins[i].inverseBindMatrices;

		for (int32_t idx : skelAsset.skins[i].joints) {
			skel.skins[i].joints.push_back(&skel.nodes[idx]);
		}

		skel.skins[i].uniformBlock.jointCount = (float)std::min((uint32_t)skel.skins[i].joints.size(), MAX_NUM_JOINTS);
		skel.skins[i].allocator = &_allocator;

		skel.skins[i].ubo = createBuffer(sizeof(Skin::UniformBlockSkinned), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		_mainDeletionQueue.pushFunction([=]() {
			vmaDestroyBuffer(_allocator, skel.skins[i].ubo._buffer, skel.skins[i].ubo._allocation);
		});

		VkDescriptorSetAllocateInfo skinAllocInfo{};
		skinAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		skinAllocInfo.descriptorPool = _descriptorPool;
		skinAllocInfo.descriptorSetCount = 1;
		skinAllocInfo.pSetLayouts = &_skinSetLayout;

		VK_CHECK(vkAllocateDescriptorSets(_device, &skinAllocInfo, &skel.skins[i].jointsDescriptorSet));

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = skel.skins[i].ubo._buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(Skin::UniformBlockSkinned);

		VkWriteDescriptorSet descriptorWrite{ vkinit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, skel.skins[i].jointsDescriptorSet, &bufferInfo, 0) };

		vkUpdateDescriptorSets(_device, 1, &descriptorWrite, 0, nullptr);

		// allocate and write shadow descriptor set
		setupShadowDescriptorSetsSkinned(*this, skel.skins[i].ubo._buffer, _shadowGlobal.shadowJointSetLayout, skel.skins[i].jointsShadowDescriptorSet);
	}

	// skelAsset and skelPool both use same animation struct
	skel.animations = skelAsset.animations;

	for (int i = 0; i < skelAsset.animations.size(); ++i) {
		skel.animNameToIndex[skelAsset.animations[i].name] = i;
	}
}


// load mesh onto CPU then upload it to the GPU
void VulkanEngine::loadMesh(const std::string& name, const std::string& path)
{
	assets::AssetFile assetFile;
	nlohmann::json metadata;

	assets::loadBinaryFile(path.c_str(), assetFile, metadata);
	assets::MeshInfo info{ assets::readMeshInfo(metadata) };

	Mesh* mesh{ new Mesh{} };
	mesh->indices.resize(info.indexBufferSize / info.indexSize);
	mesh->vertexFormat = info.vertexFormat;


	if (info.vertexFormat == VertexFormat::DEFAULT) {

		mesh->vertices.resize(info.vertexBufferSize / sizeof(Vertex));
		assets::unpackMesh(&info, assetFile.binaryBlob.data(), (char*)mesh->vertices.data(), (char*)mesh->indices.data());
		uploadMesh(mesh);

	} else if (info.vertexFormat == VertexFormat::SKINNED) {

		mesh->verticesSkinned.resize(info.vertexBufferSize / sizeof(VertexSkinned));
		assets::unpackMesh(&info, assetFile.binaryBlob.data(), (char*)mesh->verticesSkinned.data(), (char*)mesh->indices.data());
		uploadMeshSkinned(mesh);

	} else {
		std::cout << "Error: unrecognized vertex format in VulkanEngine::loadMesh\n";
	}

	_meshes[name] = mesh;
}

void VulkanEngine::loadMeshes()
{
	namespace fs = std::filesystem;
	std::string modelsPath{ "../../asset/assets_export/models/" };

	for (const auto& modelDir : fs::directory_iterator(modelsPath)) {
		if (modelDir.is_directory()) {

			for (const auto& file : fs::directory_iterator(modelDir)) {
				std::string filepath{ file.path().generic_string() };

				if (file.is_directory() && filepath.size() >= 4 && filepath.substr(filepath.size() - 5) == "_GLTF") {
					std::string filename{ file.path().filename().generic_string() };
					std::string name{ filename.substr(0, filename.size() - 5) };
					std::cout << "Loading mesh '" << name << "'\n";

					for (const auto& meshFile : fs::directory_iterator(file)) {
						if (meshFile.path().extension() == ".mesh") {
							loadMesh(name, meshFile.path().generic_string());
						}
					}

					for (const auto& skelFile : fs::directory_iterator(file)) {
						if (skelFile.path().extension() == ".skel") {
							loadSkeletalAnimation(name, skelFile.path().generic_string());
						}
					}
				}
			}
		}
	}
}

void VulkanEngine::loadTexture(const std::string& path, VkFormat format)
{
	Texture texture;
	uint32_t mipLevels;
	bool hdri{ format == VK_FORMAT_R32G32B32A32_SFLOAT };

	if (hdri) {
		std::string prefix{ "../../asset/assets/models/" };
		if (!vkutil::loadImageFromFile(*this, (prefix + path).c_str(), texture.image, &mipLevels, format)) {
			std::cout << "Failed to load texture: " << path << "\n";
		}
	} else {
		std::string prefix{ "../../asset/assets_export/models/" };
		if (!vkutil::loadImageFromAsset(*this, (prefix + path).c_str(), format, &mipLevels, texture.image)) {
			std::cout << "Failed to load texture: " << path << "\n";
		}
	}

	VkImageViewCreateInfo imageInfo{ vkinit::imageviewCreateInfo(format, texture.image._image, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels) };
	vkCreateImageView(_device, &imageInfo, nullptr, &texture.imageView);

	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyImageView(_device, texture.imageView, nullptr);
	});

	texture.mipLevels = mipLevels;

	_loadedTextures[path] = texture;
}

void VulkanEngine::loadMaterials()
{
	std::string prefix{ "../../shaders/spirv/" };
	std::string loadFile{ "../../shaders/_load_materials.txt" };
	std::ifstream file{ loadFile };
	std::string line;

	std::unordered_map<std::string, VkDescriptorType> stringToType;
	stringToType["COMBINED_IMAGE_SAMPLER"] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	std::unordered_map<std::string, VkShaderStageFlagBits> stringToStage;
	stringToStage["VERTEX"] = VK_SHADER_STAGE_VERTEX_BIT;
	stringToStage["FRAGMENT"] = VK_SHADER_STAGE_FRAGMENT_BIT;
	std::unordered_map<std::string, VkFormat> stringToFormat;
	stringToFormat["R8G8B8A8_SRGB"] = VK_FORMAT_R8G8B8A8_SRGB;
	stringToFormat["R8G8B8A8_UNORM"] = VK_FORMAT_R8G8B8A8_UNORM;
	stringToFormat["R32G32B32A32_SFLOAT"] = VK_FORMAT_R32G32B32A32_SFLOAT; // hdri


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
			} else if (field == "attr:") {
				std::string flags;
				ss >> flags;
				info.attributeFlags = std::stoul(flags);
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
							loadTexture(bindingPaths.back(), stringToFormat[format]);
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
			info.bindingTextures = texturesFromBindingPaths(bindingPaths);
			initPipeline(info, prefix);
		}

		if (cubemapTexName != "") {
			VkExtent2D textureRes{};
			textureRes.width = cubemapRes;
			textureRes.height = cubemapRes;
			bool useMip{ useMipmap == "true" };
			bool isCube{ isCubemap == "true" };

			Material* cubemapMat{ getMaterial(cubemapMaterial) };
			Texture cubemap{ renderToTexture(*this, cubemapMat->textureSet, textureRes, useMip, isCube, cubeVertPath, cubeFragPath) };

			_loadedTextures[cubemapTexName] = cubemap;
		}
	}

	file.close();
}

// determine vertex stride from the vertex attributes
uint32_t strideFromAttributes(uint32_t attr) {
	uint32_t sum{ 0 };
	if (attr & ATTR_POSITION) sum += sizeof(Vertex::position);
	if (attr & ATTR_NORMAL) sum += sizeof(Vertex::normal);
	if (attr & ATTR_TANGENT) sum += sizeof(Vertex::tangent);
	if (attr & ATTR_UV) sum += sizeof(Vertex::uv);
	if (attr & ATTR_JOINT_INDICES) sum += sizeof(VertexSkinned::jointIndices);
	if (attr & ATTR_JOINT_WEIGHTS) sum += sizeof(VertexSkinned::jointWeights);

	// Hacky, but we don't want to screw up cubemap atm
	return std::max(sum, (uint32_t)sizeof(Vertex));
}

void VulkanEngine::initPipeline(const MaterialCreateInfo& info, const std::string& prefix)
{
	VkShaderModule vertShader;
	if (!loadShaderModule(info.vertPath, &vertShader)) {
		std::cout << "Error when building vertex shader module: " << info.vertPath << "\n";
	}

	VkShaderModule fragShader;
	if (!loadShaderModule(info.fragPath, &fragShader)) {
		std::cout << "Error when building fragment shader module: " << info.fragPath << "\n";
	}

	VkPipelineLayoutCreateInfo pipeline_layout_info{ vkinit::pipelineLayoutCreateInfo() };
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

	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyDescriptorSetLayout(_device, materialSetLayout, nullptr);
	});

	std::vector<VkDescriptorSetLayout> setLayouts{ _globalSetLayout, _objectSetLayout, materialSetLayout };
	// if vertices have joint indices then they must be skinned. So we push back the skin set layout for this material
	if (info.attributeFlags & ATTR_JOINT_INDICES) {
		setLayouts.push_back(_skinSetLayout);
	}

	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pPushConstantRanges = &push_constant;
	pipeline_layout_info.setLayoutCount = setLayouts.size();
	pipeline_layout_info.pSetLayouts = setLayouts.data();

	VkPipelineLayout layout;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &layout));

	PipelineBuilder pipelineBuilder;
	// vertex input controls how to read vertices from vertex buffers
	pipelineBuilder._vertexInputInfo = vkinit::vertexInputStateCreateInfo();
	// input assembly is the configuration for drawing triangle lists, strips, or individual points
	pipelineBuilder._inputAssembly = vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	// ---------------------------------------------------------------------------------------
	// NOTE: Viewport and scissors are ignored atm, since we use dynamic viewport and scissors
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;
	pipelineBuilder._scissor.offset = { 0, 0 };
	pipelineBuilder._scissor.extent = _windowExtent;
	// ---------------------------------------------------------------------------------------

	pipelineBuilder._rasterizer = vkinit::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
	pipelineBuilder._multisampling = vkinit::multisamplingStateCreateInfo(_msaaSamples, 0.5f);
	pipelineBuilder._colorBlendAttachment = vkinit::colorBlendAttachmentState();
	pipelineBuilder._depthStencil = vkinit::depthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	VertexInputDescription vertexDescription{ getVertexDescription(info.attributeFlags, strideFromAttributes(info.attributeFlags)) };

	// connect the pipeline builder vertex input info to the one we get from Vertex
	pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();
	pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineBuilder._pipelineLayout = layout;

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShader)
	);

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader)
	);

	VkPipeline pipeline{ pipelineBuilder.buildPipeline(_device, _renderPass, true) };

	createMaterial(info, pipeline, layout, materialSetLayout);

	vkDestroyShaderModule(_device, vertShader, nullptr);
	vkDestroyShaderModule(_device, fragShader, nullptr);

	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyPipeline(_device, pipeline, nullptr);
		vkDestroyPipelineLayout(_device, layout, nullptr);
	});
}

size_t VulkanEngine::padUniformBufferSize(size_t originalSize)
{
	// Calculate required alignment based on minimum device offset alignment
	size_t minUboAlignment{ _gpuProperties.limits.minUniformBufferOffsetAlignment };
	size_t alignedSize{ originalSize };
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

void VulkanEngine::initDescriptorPool() {
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
	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
	});
}

void VulkanEngine::initObjectBuffers() {
	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		_frames[i].objectBuffer = createBuffer(sizeof(RenderObject::RenderObjectUB) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		_mainDeletionQueue.pushFunction([=]() {
			vmaDestroyBuffer(_allocator, _frames[i].objectBuffer._buffer, _frames[i].objectBuffer._allocation);
		});
	}
}

void VulkanEngine::initDescriptors()
{
	// cameraBind needs to be accessed from fragment shader to get camPos
	VkDescriptorSetLayoutBinding cameraBind{ vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };
	VkDescriptorSetLayoutBinding sceneBind{ vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1) };
	VkDescriptorSetLayoutBinding shadowMapBind{ vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2) };

	std::array<VkDescriptorSetLayoutBinding, 3> globalBindings{ cameraBind, sceneBind, shadowMapBind };

	VkDescriptorSetLayoutCreateInfo globalSetInfo{};
	globalSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	globalSetInfo.pNext = nullptr;
	globalSetInfo.flags = 0;
	globalSetInfo.bindingCount = globalBindings.size();
	globalSetInfo.pBindings = globalBindings.data();

	VkDescriptorSetLayoutBinding objectBind{ vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };

	VkDescriptorSetLayoutCreateInfo objectSetInfo{};
	objectSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	objectSetInfo.pNext = nullptr;
	objectSetInfo.flags = 0;
	objectSetInfo.bindingCount = 1;
	objectSetInfo.pBindings = &objectBind;

	VkDescriptorSetLayoutBinding skinBind{ vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };

	VkDescriptorSetLayoutCreateInfo skinSetInfo{};
	skinSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	skinSetInfo.pNext = nullptr;
	skinSetInfo.flags = 0;
	skinSetInfo.bindingCount = 1;
	skinSetInfo.pBindings = &skinBind;

	VK_CHECK(vkCreateDescriptorSetLayout(_device, &globalSetInfo, nullptr, &_globalSetLayout));
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &objectSetInfo, nullptr, &_objectSetLayout));
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &skinSetInfo, nullptr, &_skinSetLayout));

	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _skinSetLayout, nullptr);
	});

	const size_t sceneParamBufferSize{ FRAME_OVERLAP * padUniformBufferSize(sizeof(GPUSceneData)) };
	_sceneParameterBuffer = createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	_mainDeletionQueue.pushFunction([=]() {
		vmaDestroyBuffer(_allocator, _sceneParameterBuffer._buffer, _sceneParameterBuffer._allocation);
	});

	// skin descriptor set will be allocated upon loading skin

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {

		_frames[i].cameraBuffer = createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		_mainDeletionQueue.pushFunction([=]() {
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
		shadowMapInfo.imageView = _frames[i].shadow.depth.imageView;
		shadowMapInfo.sampler = _frames[i].shadow.depthSampler;

		VkDescriptorBufferInfo objectInfo{};
		objectInfo.buffer = _frames[i].objectBuffer._buffer;
		objectInfo.offset = 0;
		objectInfo.range = sizeof(RenderObject::RenderObjectUB) * MAX_OBJECTS;

		VkWriteDescriptorSet cameraWrite{ vkinit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _frames[i].globalDescriptor, &cameraInfo, 0) };
		VkWriteDescriptorSet sceneWrite{ vkinit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _frames[i].globalDescriptor, &sceneInfo, 1) };
		VkWriteDescriptorSet shadowMapWrite{ vkinit::writeDescriptorImage(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _frames[i].globalDescriptor, &shadowMapInfo, 2) };
		VkWriteDescriptorSet objectWrite{ vkinit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _frames[i].objectDescriptor, &objectInfo, 0) };
		std::array<VkWriteDescriptorSet, 4> setWrites{ cameraWrite, sceneWrite, shadowMapWrite, objectWrite };
		vkUpdateDescriptorSets(_device, setWrites.size(), setWrites.data(), 0, nullptr);
	}
}

void VulkanEngine::initSyncStructures()
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
	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyFence(_device, _uploadContext._uploadFence, nullptr);
	});

	_renderSemaphores.resize(_swapchainImages.size());
	for (auto i{ 0 }; i < _swapchainImages.size(); ++i) {
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphores[i]));

		_mainDeletionQueue.pushFunction([=]() {
			vkDestroySemaphore(_device, _renderSemaphores[i], nullptr);
		});
	}

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i].renderFence));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].presentSemaphore));

		_mainDeletionQueue.pushFunction([=]() {
			vkDestroyFence(_device, _frames[i].renderFence, nullptr);
			vkDestroySemaphore(_device, _frames[i].presentSemaphore, nullptr);
		});
	}
}

void VulkanEngine::initDefaultRenderpass()
{
	VkAttachmentDescription color_attachment{};
	// the attachment will have the format needed by the swapchain
	color_attachment.format = _swapchainImageFormat;
	color_attachment.samples = _msaaSamples;
	// we clear when this attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// we keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// we don't care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// we don't know or care about the starting layout of the attachment
	color_attachment.initialLayout - VK_IMAGE_LAYOUT_UNDEFINED;
	// This attachment won't be presented, it will be resolved to the swapchain image
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference color_attachment_ref{};
	// attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	// the driver will transition to this layout for us as the start of this subpass
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depth_attachment{};
	depth_attachment.flags = 0;
	depth_attachment.format = _depthFormat;
	depth_attachment.samples = _msaaSamples;
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

	VkAttachmentDescription color_attachment_resolve{};
	color_attachment_resolve.format = _swapchainImageFormat;
	color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment_resolve.initialLayout - VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_resolve_ref{};
	color_attachment_resolve_ref.attachment = 2;
	color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;
	subpass.pResolveAttachments = &color_attachment_resolve_ref;

	std::array<VkAttachmentDescription, 3> attachments{ color_attachment, depth_attachment, color_attachment_resolve };

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
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

	render_pass_info.dependencyCount = dependencies.size();
	render_pass_info.pDependencies = dependencies.data();

	VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));

	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyRenderPass(_device, _renderPass, nullptr);
	});
}

void VulkanEngine::initFramebuffers(bool windowResize)
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

		std::array<VkImageView, 3> attachments{
			_colorImageView,
			_depthImageView,
			_swapchainImageViews[i],
		};

		fb_info.attachmentCount = attachments.size();
		fb_info.pAttachments = attachments.data();
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

		if (!windowResize) {
			_mainDeletionQueue.pushFunction([=]() {
				vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
				vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
			});
		}
	}
}

void VulkanEngine::initCommands()
{
	// create a command pool for commands submitted to the graphics queue
	// we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo{ vkinit::commandPoolCreateInfo(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) };
	VkCommandPoolCreateInfo uploadCommandPoolInfo{ vkinit::commandPoolCreateInfo(_graphicsQueueFamily) };

	// create pool for upload context
	VK_CHECK(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_uploadContext._commandPool));

	_mainDeletionQueue.pushFunction([=]() {
		vkDestroyCommandPool(_device, _uploadContext._commandPool, nullptr);
	});

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i].commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo{ vkinit::commandBufferAllocateInfo(_frames[i].commandPool, 1) };
		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i].mainCommandBuffer));

		_mainDeletionQueue.pushFunction([=]() {
			vkDestroyCommandPool(_device, _frames[i].commandPool, nullptr);
		});
	}
}

void VulkanEngine::initSwapchain(VkSwapchainKHR oldSwapChain)
{
	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU, _device, _surface };

	vkb::Swapchain vkbSwapchain{ swapchainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
		//.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.set_old_swapchain(oldSwapChain)
		.build()
		.value() };

	// store the swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
	_swapchainImageFormat = vkbSwapchain.image_format;

	if (oldSwapChain == VK_NULL_HANDLE) {
		_mainDeletionQueue.pushFunction([=]() {
			vkDestroySwapchainKHR(_device, _swapchain, nullptr);
		});
	}

	// depth image size will match the window
	VkExtent3D depthImageExtent{
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	// hardcoding the depth format to the 32 bit float
	_depthFormat = VK_FORMAT_D32_SFLOAT;

	// the depth image will be an image with the format we selected and Depth Attachment usage flag
	VkImageCreateInfo dimg_info{ vkinit::imageCreateInfo(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent, 1, _msaaSamples) };
	VkImageCreateInfo color_img_info{ vkinit::imageCreateInfo(vkbSwapchain.image_format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, depthImageExtent, 1, _msaaSamples) };


	// for the depth image, we want to allocate it from GPU local memory
	VmaAllocationCreateInfo dimg_allocinfo{};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dimg_allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VmaAllocationCreateInfo color_img_allocinfo{};
	color_img_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	color_img_allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr);
	vmaCreateImage(_allocator, &color_img_info, &color_img_allocinfo, &_colorImage._image, &_colorImage._allocation, nullptr);

	// build an imageView for the depth image to use for rendering
	VkImageViewCreateInfo dview_info{ vkinit::imageviewCreateInfo(_depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT, 1) };
	VkImageViewCreateInfo color_view_info{ vkinit::imageviewCreateInfo(vkbSwapchain.image_format, _colorImage._image, VK_IMAGE_ASPECT_COLOR_BIT, 1) };

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));
	VK_CHECK(vkCreateImageView(_device, &color_view_info, nullptr, &_colorImageView));

	if (oldSwapChain == VK_NULL_HANDLE) {
		_mainDeletionQueue.pushFunction([=]() {
			vkDestroyImageView(_device, _colorImageView, nullptr);
			vkDestroyImageView(_device, _depthImageView, nullptr);
			vmaDestroyImage(_allocator, _colorImage._image, _colorImage._allocation);
			vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
		});
	}
}

void VulkanEngine::initVulkan()
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

	VkPhysicalDeviceFeatures features{};
	features.sampleRateShading = VK_TRUE;

	// use vkbootstrap to select a gpu.
	// we want a gpu that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice{ selector
		.set_minimum_version(1, 1)
		.set_surface(_surface)
		.set_required_features(features)
		.select()
		.value() };


	// create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	vkb::Device vkbDevice{ deviceBuilder.build().value() };

	// get the VKDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

	// max number of samples GPU supports for both color and depth
	_msaaSamples = getMaxUsableSampleCount(_chosenGPU);

	// use vkbootstrap to get a graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_mainDeletionQueue.pushFunction([=]() {
		vmaDestroyAllocator(_allocator);
	});

	vkGetPhysicalDeviceProperties(_chosenGPU, &_gpuProperties);
}

bool VulkanEngine::loadShaderModule(const std::string& filePath, VkShaderModule* outShaderModule)
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
AllocatedBuffer VulkanEngine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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

Material* VulkanEngine::createMaterial(const MaterialCreateInfo& info, VkPipeline pipeline, VkPipelineLayout layout, VkDescriptorSetLayout materialSetLayout)
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
		VkSamplerCreateInfo samplerInfo{ vkinit::samplerCreateInfo(VK_FILTER_LINEAR, texture.mipLevels, VK_SAMPLER_ADDRESS_MODE_REPEAT) };
		VkSampler sampler;
		vkCreateSampler(_device, &samplerInfo, nullptr, &sampler);

		_mainDeletionQueue.pushFunction([=]() {
			vkDestroySampler(_device, sampler, nullptr);
		});

		VkDescriptorImageInfo imageBufferInfo{};
		imageBufferInfo.sampler = sampler;
		imageBufferInfo.imageView = texture.imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet textureWrite{ vkinit::writeDescriptorImage(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mat.textureSet, &imageBufferInfo, i) };

		vkUpdateDescriptorSets(_device, 1, &textureWrite, 0, nullptr);
	}

	_materials[info.name] = mat;
	return &_materials[info.name];
}

std::vector<Texture> VulkanEngine::texturesFromBindingPaths(const std::vector<std::string>& bindingPaths)
{
	std::vector<Texture> result;
	result.reserve(bindingPaths.size());

	for (const std::string& bindingPath : bindingPaths) {
		result.push_back(_loadedTextures[bindingPath]);
	}

	return result;
}

Material* VulkanEngine::getMaterial(const std::string& name)
{
	// search for the material and return nullptr if not found
	auto it{ _materials.find(name) };
	if (it == _materials.end()) {
		return nullptr;
	} else {
		return &(it->second);
	}
}

Mesh* VulkanEngine::getMesh(const std::string& name)
{
	auto it{ _meshes.find(name) };
	if (it == _meshes.end()) {
		std::cout << " Could not find mesh '" << name << "' Did you remember to export the .gltf file and bake?\n";
		return nullptr;
	} else {
		return it->second;
	}
}

FrameData& VulkanEngine::getCurrentFrame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
}

void VulkanEngine::initShadowPass()
{
	_shadowGlobal.width = SHADOWMAP_DIM;
	_shadowGlobal.height = SHADOWMAP_DIM;

	prepareShadowMapRenderpass(*this, &_shadowGlobal.renderPass);

	std::vector<VkDescriptorSetLayout> setLayouts{};
	std::vector<VkDescriptorSetLayout> setLayoutsSkinned{};
	setupShadowDescriptorSetLayouts(*this, setLayouts, &_shadowGlobal.shadowPipelineLayout);
	setupShadowDescriptorSetLayoutsSkinned(*this, setLayoutsSkinned, &_shadowGlobal.shadowPipelineLayoutSkinned);

	_shadowGlobal.shadowJointSetLayout = setLayoutsSkinned[2];

	initShadowPipeline(*this, _shadowGlobal.renderPass, _shadowGlobal.shadowPipelineLayout, &_shadowGlobal.shadowPipeline);
	initShadowPipelineSkinned(*this, _shadowGlobal.renderPass, _shadowGlobal.shadowPipelineLayoutSkinned, &_shadowGlobal.shadowPipelineSkinned);

	for (auto i{ 0 }; i < FRAME_OVERLAP; ++i) {
		ShadowFrameResources& shadowFrame{ _frames[i % FRAME_OVERLAP].shadow };

		prepareShadowMapFramebuffer(*this, _shadowGlobal, &shadowFrame);

		shadowFrame.shadowLightBuffer = createBuffer(sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_mainDeletionQueue.pushFunction([=]() {
			vmaDestroyBuffer(_allocator, shadowFrame.shadowLightBuffer._buffer, shadowFrame.shadowLightBuffer._allocation);
		});

		// Set up all global shadow descriptor sets common to all shadows.
		// We set up the skinned descriptor set when we load the skin in loadSkeletalAnimation, since it's a per-skin descriptor set.
		setupShadowDescriptorSetsGlobal(*this, shadowFrame, _frames[i].objectBuffer._buffer, setLayouts);
	}
}

// Minimal bounding sphere of view frustum. Only needs to be called when window is resized
// or at startup. From:
// https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
void VulkanEngine::initBoundingSphere() {
	float widthHeightRatio{ _windowExtent.height / (float)_windowExtent.width };
	float k{ std::sqrtf(1.0f + widthHeightRatio * widthHeightRatio) * std::tanf(glm::radians(FOV) / 2.0) };
	float k2{ k * k };
	float n{ NEAR_PLANE };
	float f{ FAR_PLANE_SHADOW };

	if (k2 >= (f - n) / (f + n)) {
		_boundingSphereZ = -f;
		_boundingSphereR = f * k;
	} else {
		_boundingSphereZ = -0.5f * (f + n) * (1 + k2);
		_boundingSphereR = 0.5f * std::sqrtf((f - n) * (f - n) + 2 * (f * f + n * n) * k2 + (f + n) * (f + n) * k2 * k2);
	}
}

// First render pass: Generate shadow map by rendering the scene from light's POV
void VulkanEngine::shadowPass(VkCommandBuffer& cmd)
{
	VkClearValue clearValue{};
	clearValue.color = { {1.00, 0.00, 0.00, 1.0} };

	VkClearValue depthClear{};
	depthClear.depthStencil.depth = 1.0f;

	std::array<VkClearValue, 2> clearValues{ clearValue, depthClear };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = _shadowGlobal.renderPass;
	renderPassBeginInfo.framebuffer = getCurrentFrame().shadow.frameBuffer;
	renderPassBeginInfo.renderArea.extent.width = _shadowGlobal.width;
	renderPassBeginInfo.renderArea.extent.height = _shadowGlobal.height;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.width = (float)_shadowGlobal.width;
	viewport.height = (float)_shadowGlobal.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0;
	viewport.y = 0;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent.width = _shadowGlobal.width;
	scissor.extent.height = _shadowGlobal.height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	float near_plane{ 0.0f };
	float far_plane{ 2.0f * _boundingSphereR };

	float scale{ _boundingSphereR };
	glm::mat4 lightProjection{ vkutil::ortho(-scale, scale, -scale, scale, near_plane, far_plane) };

	glm::vec3 center{ 0.0, 0.0, _boundingSphereZ };
	center = _viewInv * glm::vec4{ center, 1.0f };
	glm::vec3 dir{ glm::normalize(glm::vec3{ -4.0f, -8.0f, -2.0f }) };

	float halfLength{ (far_plane - near_plane) / 2.0f };

	glm::mat4 rotate{ glm::rotation(glm::vec3{ 0.0, 0.0, -1.0 }, dir) };
	//glm::mat4 translate{ glm::translate(glm::vec3{ 4.0f, 8.0f, 2.0f }) };
	glm::mat4 translate{ glm::translate(center - halfLength * dir) };
	glm::mat4 lightView{ translate * rotate };
	lightView = glm::inverse(lightView);

	_shadowGlobal.lightSpaceMatrix = lightProjection * lightView;

	void* data;
	vmaMapMemory(_allocator, getCurrentFrame().shadow.shadowLightBuffer._allocation, &data);
	std::memcpy(data, &_shadowGlobal.lightSpaceMatrix, sizeof(glm::mat4));
	vmaUnmapMemory(_allocator, getCurrentFrame().shadow.shadowLightBuffer._allocation);

	// Set depth bias (aka "Polygon offset")
	// Required to avoid shadow mapping artifacts
	vkCmdSetDepthBias(cmd, _shadowGlobal.depthBiasConstant, 0.0f, _shadowGlobal.depthBiasSlope);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowGlobal.shadowPipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowGlobal.shadowPipelineLayout, 0, 1, &getCurrentFrame().shadow.shadowDescriptorSetLight, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowGlobal.shadowPipelineLayout, 1, 1, &getCurrentFrame().shadow.shadowDescriptorSetObjects, 0, nullptr);

	Mesh* lastMesh{ nullptr };
	bool lastSkinned{ false };

	uint32_t idx{ 0 };
	for (const RenderObject& object : _renderables) {

		// DO NOT change this to continue if !object.castShadow, because we need to increment idx still
		if (object.castShadow) {
			bool isSkinned{ object.mesh->vertexFormat == VertexFormat::SKINNED };

			if (lastSkinned != isSkinned) {
				VkPipeline pipeline{ isSkinned ? _shadowGlobal.shadowPipelineSkinned : _shadowGlobal.shadowPipeline };
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				lastSkinned = isSkinned;
			}

			if (isSkinned) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowGlobal.shadowPipelineLayoutSkinned, 2, 1, &object.mesh->skel.skins[0].jointsShadowDescriptorSet, 0, nullptr);
			}

			// only bind the mesh if it's a different one from last bind
			if (object.mesh != lastMesh) {
				// bind the mesh vertex buffer with offset 0
				VkDeviceSize offset{ 0 };
				vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->vertexBuffer._buffer, &offset);
				vkCmdBindIndexBuffer(cmd, object.mesh->indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT16);

				lastMesh = object.mesh;
			}

			//vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, idx);
			vkCmdDrawIndexed(cmd, object.mesh->indices.size(), 1, 0, 0, idx);
		}

		++idx;
	}

	vkCmdEndRenderPass(cmd);
}

void VulkanEngine::cameraTransformation()
{
	glm::mat4 view{ _camTransform.mat4() };
	glm::mat4 viewOrigin{ _camTransform.rot }; // for skybox
	_viewInv = view; // for use in shadowpass with sun shadow

	view = glm::inverse(view);
	viewOrigin = glm::inverse(viewOrigin);

	glm::mat4 projection{ glm::infinitePerspective(glm::radians(FOV), _windowExtent.width / (float)_windowExtent.height, NEAR_PLANE) };
	projection[1][1] *= -1;

	// fill a GPU camera data struct
	GPUCameraData camData{};
	camData.viewProjOrigin = projection * viewOrigin; // for skybox
	camData.projection = projection;
	camData.viewProj = projection * view;

	// copy camera data to camera buffer
	void* data;
	vmaMapMemory(_allocator, getCurrentFrame().cameraBuffer._allocation, &data);
	std::memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(_allocator, getCurrentFrame().cameraBuffer._allocation);
}

void VulkanEngine::draw()
{
	ImGui::Render();

	uint32_t msStartTime{ SDL_GetTicks() };

	// wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame().renderFence, true, 1'000'000'000));
	VK_CHECK(vkResetFences(_device, 1, &getCurrentFrame().renderFence));

	// request image from the swapchain, one second timeout. This is also where vsync happens according to vkguide, but for me it happens at present
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1'000'000'000, getCurrentFrame().presentSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

	// now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again
	VK_CHECK(vkResetCommandBuffer(getCurrentFrame().mainCommandBuffer, 0));

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo{};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;
	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	// Assume _camTransform and _sceneParamters lights are updated here if they need to be
	_app->update(*this, _delta);

	// write all the objects' matrices into the SSBO (used in both shadow pass and draw objects)
	void* objectData;
	vmaMapMemory(_allocator, getCurrentFrame().objectBuffer._allocation, &objectData);
	RenderObject::RenderObjectUB* objectSSBO{ (RenderObject::RenderObjectUB*)objectData };
	uint32_t idx{ 0 };
	for (const RenderObject& object : _renderables) {
		if (object.animated()) {
			object.updateAnimation(_delta);
		}
		objectSSBO[idx] = object.uniformBlock;
		++idx;
	}
	vmaUnmapMemory(_allocator, getCurrentFrame().objectBuffer._allocation);
	vmaFlushAllocation(_allocator, getCurrentFrame().objectBuffer._allocation, 0, VK_WHOLE_SIZE);

	VK_CHECK(vkBeginCommandBuffer(getCurrentFrame().mainCommandBuffer, &cmdBeginInfo));

	cameraTransformation();
	shadowPass(getCurrentFrame().mainCommandBuffer);

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

	vkCmdBeginRenderPass(getCurrentFrame().mainCommandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.width = (float)_windowExtent.width;
	viewport.height = (float)_windowExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0;
	viewport.y = 0;
	vkCmdSetViewport(getCurrentFrame().mainCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = _windowExtent;
	scissor.offset = { 0, 0 };
	vkCmdSetScissor(getCurrentFrame().mainCommandBuffer, 0, 1, &scissor);

	drawObjects(getCurrentFrame().mainCommandBuffer, _renderables);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), getCurrentFrame().mainCommandBuffer);

	vkCmdEndRenderPass(getCurrentFrame().mainCommandBuffer);
	TracyVkCollect(getCurrentFrame().tracyContext, getCurrentFrame().mainCommandBuffer);

	VK_CHECK(vkEndCommandBuffer(getCurrentFrame().mainCommandBuffer));

	// we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain
	// is ready. we will signal the _renderSemaphore, to signal that rendering has finished

	VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;
	submit.pWaitDstStageMask = &waitStage;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &getCurrentFrame().presentSemaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_renderSemaphores[swapchainImageIndex];
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &getCurrentFrame().mainCommandBuffer;

	// submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, getCurrentFrame().renderFence));

	// this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that, as it's necessary that
	// drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &_renderSemaphores[swapchainImageIndex];
	presentInfo.pImageIndices = &swapchainImageIndex;

	uint32_t msEndTime{ SDL_GetTicks() };
	_msDelta = msEndTime - msStartTime;

	// This is what actually blocks for vsync at least on my system...
	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	FrameMark;
	++_frameNumber;
}

void VulkanEngine::drawObjects(VkCommandBuffer cmd, const std::multiset<RenderObject>& renderables)
{
	TracyVkZone(getCurrentFrame().tracyContext, cmd, "Draw objects");

	_sceneParameters.lightSpaceMatrix = _shadowGlobal.lightSpaceMatrix;
	_sceneParameters.camPos = glm::vec4(_camTransform.pos, 1.0);

	// copy scene data to scene buffer
	char* sceneData;
	vmaMapMemory(_allocator, _sceneParameterBuffer._allocation, (void**)&sceneData);
	int frameIndex{ _frameNumber % FRAME_OVERLAP };
	sceneData += padUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;
	std::memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator, _sceneParameterBuffer._allocation);

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
			uint32_t uniformOffset{ static_cast<uint32_t>(padUniformBufferSize(sizeof(GPUSceneData)) * frameIndex) };
			// we probably bind descriptor set here since it depends on the pipelinelayout
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &getCurrentFrame().globalDescriptor, 1, &uniformOffset);

			// object data descriptor
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &getCurrentFrame().objectDescriptor, 0, nullptr);

			if (object.material->textureSet != VK_NULL_HANDLE) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 2, 1, &object.material->textureSet, 0, nullptr);
			}

			if (!object.mesh->skel.skins.empty()) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 3, 1, &object.mesh->skel.skins[0].jointsDescriptorSet, 0, nullptr);
			}

			++pipelineBinds;
		}

		//glm::mat4 model{ object.transformMatrix };
		//// final render matrix that we are calculating on the CPU
		//glm::mat4 mesh_matrix{ projection * view * model };

		MeshPushConstants constants{};
		constants.roughnessMultiplier = glm::vec4{ _guiData.roughness_mult };

		vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &constants);

		// only bind the mesh if it's a different one from last bind
		if (object.mesh != lastMesh) {
			// bind the mesh vertex buffer with offset 0
			VkDeviceSize offset{ 0 };
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->vertexBuffer._buffer, &offset);
			vkCmdBindIndexBuffer(cmd, object.mesh->indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT16);

			lastMesh = object.mesh;
			++vertexBufferBinds;
		}

		//vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, idx);
		vkCmdDrawIndexed(cmd, object.mesh->indices.size(), 1, 0, 0, idx);
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

	_app->gui(*this);
}

void VulkanEngine::addToPhysicsEngineDynamic(GameObject* go, PxShape* shape, float density)
{
	// if the gameobject is null, just add the physics object to the physics engine and don't associate it with a gameobject
	if (go) {
		PxRigidDynamic* physicsObject{ _physicsEngine.addToPhysicsEngineDynamic(go->getTransform().toPhysx(), shape, density) };
		go->setPhysicsObject(physicsObject);
		_physicsObjects.push_back(go);
	} else {
		_physicsEngine.addToPhysicsEngineDynamic(PxTransform{}, shape, density);
	}
}

void VulkanEngine::addToPhysicsEngineDynamicMass(GameObject* go, PxShape* shape, float mass)
{
	if (go) {
		PxRigidDynamic* physicsObject{ _physicsEngine.addToPhysicsEngineDynamicMass(go->getTransform().toPhysx(), shape, mass) };
		go->setPhysicsObject(physicsObject);
		_physicsObjects.push_back(go);
	} else {
		_physicsEngine.addToPhysicsEngineDynamicMass(PxTransform{}, shape, mass);
	}
}

void VulkanEngine::addToPhysicsEngineStatic(GameObject* go, PxShape* shape)
{
	if (go) {
		PxRigidStatic* physicsObject{ _physicsEngine.addToPhysicsEngineStatic(go->getTransform().toPhysx(), shape) };
		go->setPhysicsObject(physicsObject);
		_physicsObjects.push_back(go);
	} else {
		_physicsEngine.addToPhysicsEngineStatic(PxTransform{}, shape);
	}
}

void VulkanEngine::setGravity(float gravity)
{
	_physicsEngine.setGravity(gravity);
}

bool VulkanEngine::advancePhysics(float delta)
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

void VulkanEngine::updatePhysics()
{
	ZoneScoped;
	// Advance forward simulation
	advancePhysics(_delta);

	// Update gameobject transforms to match the transforms of the physics objects
	for (GameObject* go : _physicsObjects) {
		Transform t{ _physicsEngine.getActorTransform(go->getPhysicsObject()) };
		go->setTransform(t);
	}
}

PxMaterial* VulkanEngine::createPhysicsMaterial(float staticFriciton, float dynamicFriction, float restitution)
{
	return _physicsEngine.createMaterial(staticFriciton, dynamicFriction, restitution);
}

PxShape* VulkanEngine::createPhysicsShape(const PxGeometry& geometry, const PxMaterial& material, bool isExclusive, PxShapeFlags shapeFlags)
{
	return _physicsEngine.createShape(geometry, material, isExclusive, shapeFlags);
}

void VulkanEngine::resizeWindow(int32_t width, int32_t height)
{
	vkDeviceWaitIdle(_device);

	// destroy framebuffers
	const uint32_t swapchain_imagecount{ (uint32_t)_swapchainImages.size() };
	for (auto i{ 0 }; i < swapchain_imagecount; ++i) {
		vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}

	// destroy color and depth images
	vkDestroyImageView(_device, _colorImageView, nullptr);
	vkDestroyImageView(_device, _depthImageView, nullptr);
	vmaDestroyImage(_allocator, _colorImage._image, _colorImage._allocation);
	vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);

	_windowExtent.width = width;
	_windowExtent.height = height;

	VkSwapchainKHR oldSwapchain{ _swapchain };

	initSwapchain(_swapchain);
	initFramebuffers(true);

	// destroy old swapchain after we use it to initialize the new one
	vkDestroySwapchainKHR(_device, oldSwapchain, nullptr);

	initBoundingSphere();
}

bool VulkanEngine::input() {
	bool bQuit{ false };
	const Uint8* keystate{ SDL_GetKeyboardState(nullptr) };
	_app->input(keystate, _delta);

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		bQuit = _app->events(e);

		switch (e.type)
		{
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				std::cout << "Window resized\n";
				resizeWindow(e.window.data1, e.window.data2);
			} else if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
				std::cout << "Window minimized\n";
				_minimized = true;
			} else if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
				std::cout << "Window restored\n";
				_minimized = false;
			}
			break;
		}
	}

	return bQuit;
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

		bQuit = input();
		if (!_minimized) {
			gui();
			updatePhysics();
			draw();
			showFPS();
		}
	}
}

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkRenderPass pass, bool dynamicState)
{
	VkGraphicsPipelineCreateInfo pipelineInfo{};

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineDynamicStateCreateInfo dynamicStateCI{};
	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	if (dynamicState) {
		dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCI.dynamicStateCount = (uint32_t)dynamicStateEnables.size();
		dynamicStateCI.pDynamicStates = dynamicStateEnables.data();

		viewportState.pViewports = nullptr;
		viewportState.pScissors = nullptr;
		pipelineInfo.pDynamicState = &dynamicStateCI;

	} else {
		viewportState.pViewports = &_viewport;
		viewportState.pScissors = &_scissor;
		pipelineInfo.pDynamicState = nullptr;
	}

	// setup dummy color blending. We aren't using transparent objects yet
	// the blending is just "no blend", but we do write to the color attachment
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &_colorBlendAttachment;

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

// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#pragma warning(disable : 26812) // The enum type * is unscoped. Prefer 'enum class' over 'enum'.

#include <vector>
#include <cstdint>
#include <deque>
#include <functional>
#include <unordered_map>
#include <string>
#include <set>
#include <chrono>
#include <type_traits>

#include "vk_types.h"
#include "vk_mem_alloc.h"
#include "vk_mesh.h"
#include "vk_mesh_asset.h"
#include "glm/glm.hpp"
#include "../tracy/Tracy.hpp"		// CPU profiling
#include "../tracy/TracyVulkan.hpp"
#include "application.h"
#include "physics.h"
#include "asset_loader.h"

#define VK_CHECK(x)\
	do\
	{\
		VkResult err = x;\
		if (err) {\
			std::cout << "Detected Vulkan error: " << err << '\n';\
			__debugbreak();\
		}\
	} while (0)\

// number of frames to overlap when rendering
constexpr uint32_t FRAME_OVERLAP{ 2 };
constexpr size_t MAX_NUM_TOTAL_LIGHTS{ 10 }; // this must match glsl shader!
constexpr uint32_t SHADOWMAP_DIM{ 4096 };
constexpr uint32_t MAX_OBJECTS{ 10000 };
constexpr float FOV{ 70.0f }; // degrees
constexpr float NEAR_PLANE{ 0.05f };
constexpr float FAR_PLANE_SHADOW{ 25.0f }; // Rendering has an inf far plane, this is only used for shadow maps

struct VulkanEngine;

struct Light {
	glm::vec4 position; // w is unused
	glm::vec4 color;    // w is for intensity
};

struct DirectionLight {
	Light light;
	glm::vec4 direction;
};

struct GPUSceneData {
	glm::mat4 lightSpaceMatrix;
	glm::vec4 camPos; // w is unused
	Light lights[MAX_NUM_TOTAL_LIGHTS];
	uint32_t numLights;
};

struct GPUCameraData {
	glm::mat4 viewProjOrigin;
	glm::mat4 projection;
	glm::mat4 viewProj;
};

struct ShadowGlobalResources {
	uint32_t width;
	uint32_t height;
	VkRenderPass renderPass;
	// Depth bias (and slope) are used to avoid shadowing artifacts
	// Constant depth bias factor (always applied)
	float depthBiasConstant{ 1.25f };
	// Slope depth bias factor, applied depending on polygon's slope
	float depthBiasSlope{ 1.75f };
	VkPipeline shadowPipeline;
	VkPipeline shadowPipelineSkinned;
	VkPipelineLayout shadowPipelineLayout;
	VkPipelineLayout shadowPipelineLayoutSkinned;
	VkDescriptorSetLayout shadowJointSetLayout;
	glm::mat4 lightSpaceMatrix;
};

struct ShadowFrameResources {
	VkFramebuffer frameBuffer;
	Texture depth;
	VkSampler depthSampler;
	VkDescriptorImageInfo descriptor;
	VkPipelineLayout shadowPipelineLayout;
	// Notice that there is not a descriptor set for skin here. That is in Skin struct.
	VkDescriptorSet shadowDescriptorSetLight;
	VkDescriptorSet shadowDescriptorSetObjects;
	AllocatedBuffer shadowLightBuffer;
};

struct FrameData {
	VkSemaphore presentSemaphore;
	VkFence renderFence;

	// This belongs to a frame because it's fast to reset a whole
	// command pool, and we reset this for the whole frame
	// (aka lifetime of this command pool is lifetime of this frame)
	VkCommandPool commandPool;
	// command buffer belongs to frame since we for the next frame
	// while the other frame's command buffer is submitted
	VkCommandBuffer mainCommandBuffer;

	// buffer that holds a single GPUCameraData to use when rendering
	AllocatedBuffer cameraBuffer;
	// descriptor that has frame lifetime
	VkDescriptorSet globalDescriptor;

	// Object matrices for all objects in scenes. This belongs to the frame since it's
	// dynamic and changing every frame (static scenes would need only one objectBuffer)
	AllocatedBuffer objectBuffer;
	VkDescriptorSet objectDescriptor;

	TracyVkCtx tracyContext;

	ShadowFrameResources shadow;
};

struct Transform {
	glm::vec3 pos{ 0.0 };
	glm::vec3 scale{ 1.0 };
	glm::mat4 rot{ 1.0 };

	Transform();
	Transform(const PxTransform& pxt);
	glm::mat4 mat4();
	physx::PxTransform toPhysx();
};

class GameObject {
public:
	GameObject(const RenderObject* ro)
		: _renderObject{ ro }
		, _transform{}
		, _parent{ nullptr }
		, _physicsObject{}
	{}

	GameObject()
		: _renderObject{ nullptr }
		, _transform{}
		, _parent{ nullptr }
		, _physicsObject{}
	{}

	void playAnimation(const std::string& name);

	void setRenderObject(const RenderObject* ro);

	physx::PxRigidActor* getPhysicsObject();

	void setPhysicsObject(physx::PxRigidActor* body);

	Transform getTransform();

	glm::mat4 getGlobalMat4();

	glm::vec3 getPos();

	glm::vec3 getScale();

	glm::mat4 getRot();

	void setTransform(Transform transform);

	void setPos(glm::vec3 pos);

	void setScale(glm::vec3 scale);

	void setRot(glm::mat4 rot);

	void setForceStepInterpolation(bool x);

	void setParent(GameObject* parent);

	// physics

	void addForce(glm::vec3 force);

	void addTorque(glm::vec3 torque);

	void setVelocity(glm::vec3 velocity);

	void setPhysicsTransform(Transform transform);

	void setMass(float mass);
	
	float getMass();

private:
	void updateRenderMatrix();

	Transform _transform;
	const RenderObject* _renderObject;
	physx::PxRigidActor* _physicsObject;
	GameObject* _parent;
	std::vector<GameObject*> _children;
};

struct GuiData {
	Light light0;
	Light light1;
	float bedAngle;
	const RenderObject* bed;
	float roughness_mult;
};

struct MeshPushConstants {
	glm::vec4 roughnessMultiplier; // only x component is used
	glm::mat4 renderMatrix;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void pushFunction(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it{ deletors.rbegin() }; it != deletors.rend(); ++it) {
			(*it)(); // call functors
		}

		deletors.clear();
	}
};

class VulkanEngine {
public:

	// We need each of these per immediate submission since we will allocate a buffer
	// from this command pool and use a fence to wait for it to finish.
	// we use a separate command pool from rendering so we can reset it and use
	// it from separate threads
	struct UploadContext {
		VkFence _uploadFence;
		VkCommandPool _commandPool;
	};

	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	VkExtent2D _windowExtent{ 1700 , 900 };
	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;

	VkSwapchainKHR _swapchain;
	// image format expected by the windowing system
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	std::vector<VkSemaphore> _renderSemaphores;

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	VkRenderPass _renderPass;
	std::vector<VkFramebuffer> _framebuffers;

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator;

	VkImageView _depthImageView;
	AllocatedImage _depthImage;
	VkFormat _depthFormat;

	std::multiset<RenderObject> _renderables;
	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh*> _meshes;

	Transform _camTransform{};

	VkDescriptorSetLayout _globalSetLayout;
	VkDescriptorPool _descriptorPool;

	VkPhysicalDeviceProperties _gpuProperties;

	GPUSceneData _sceneParameters;
	AllocatedBuffer _sceneParameterBuffer;

	VkDescriptorSetLayout _objectSetLayout;
	VkDescriptorSetLayout _skinSetLayout;

	UploadContext _uploadContext;

	//texture hashmap
	std::unordered_map<std::string, Texture> _loadedTextures;

	GuiData _guiData;

	// frame storage
	FrameData _frames[FRAME_OVERLAP];

	ShadowGlobalResources _shadowGlobal;
	float _boundingSphereZ;
	float _boundingSphereR;
	glm::mat4 _viewInv;

	VkSampleCountFlagBits _msaaSamples;
	AllocatedImage _colorImage;
	VkImageView _colorImageView;

	std::vector<GameObject*> _physicsObjects;

	Application* _app;
	PhysicsEngine _physicsEngine;

	float _physicsAccumulator{ 0.0f };
	float _physicsStepSize{ 1.0f / 60.0f };

	// for delta time
	std::chrono::steady_clock::time_point _lastTime{};
	float _delta{ 0.0f };

	// profiling variables
	double _lastTimeFPS{ 0.0 };
	uint32_t _nbFrames{ 0 };

	// measure ms of each frame without vsync
	double _msDelta;

	bool _minimized{ false };

	// initializes everything in the engine
	void init(Application* app);

	// shuts down the engine
	void cleanup();

	// draw loop
	void draw();

	// run main loop
	void run();

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	void loadTexture(const std::string& path, VkFormat format);

	bool loadShaderModule(const std::string& filePath, VkShaderModule* outShaderModule);

	// create material and add it to the map
	Material* createMaterial(const MaterialCreateInfo& info, VkPipeline pipeline, VkPipelineLayout layout, VkDescriptorSetLayout materialSetLayout);

	// returns nullptr if it can't be found
	Mesh* getMesh(const std::string& name);

	const RenderObject* createRenderObject(const std::string& meshName, const std::string& matName, bool castShadow=true);

	const RenderObject* createRenderObject(const std::string& name);

	void setCameraTransform(Transform transform);

	void setSceneLights(const std::vector<Light>& lights);

	void addToPhysicsEngineDynamic(GameObject* go, PxShape* shape, float density = 10.0f);

	void addToPhysicsEngineDynamicMass(GameObject* go, PxShape* shape, float mass);

	void addToPhysicsEngineStatic(GameObject* go, PxShape* shape);

	void updatePhysics();

	bool advancePhysics(float delta);

	PxMaterial* createPhysicsMaterial(float staticFriciton, float dynamicFriction, float restitution);

	PxShape* createPhysicsShape(const PxGeometry& geometry,
		const PxMaterial& material,
		bool isExclusive = false,
		PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE);

	void setGravity(float gravity);

	void resizeWindow(int32_t width, int32_t height);

private:

	void initVulkan();

	void initSwapchain(VkSwapchainKHR oldSwapChain);

	void initCommands();

	void initDefaultRenderpass();

	void initFramebuffers(bool windowResize);

	void initSyncStructures();

	void initPipeline(const MaterialCreateInfo& info, const std::string& prefix);

	void loadMeshes();

	void loadMaterials();

	void showFPS();

	void initScene();

	// getter for the frame we are rendering to right now
	FrameData& getCurrentFrame();

	// returns nullptr if it can't be found
	Material* getMaterial(const std::string& name);

	void drawObjects(VkCommandBuffer cmd, const std::multiset<RenderObject>& renderables);

	void initDescriptors();

	void initObjectBuffers();

	void initDescriptorPool();

	size_t padUniformBufferSize(size_t originalSize);

	void initImgui();

	void gui();

	void initGuiData();

	std::vector<Texture> texturesFromBindingPaths(const std::vector<std::string>& bindingPaths);

	void initTracy();

	void shadowPass(VkCommandBuffer& cmd);

	void initShadowPass();

	VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

	bool input();

	void initBoundingSphere();

	void cameraTransformation();

	void loadMesh(const std::string& name, const std::string& path);

	void loadSkeletalAnimation(const std::string& name, const std::string& path);

	void uploadMesh(Mesh* mesh);

	void uploadMeshSkinned(Mesh* mesh);

	// For use with vertex buffers or index buffers. If !isVertexBuffer, then index buffer is assumed
	template <typename T>
	void uploadBuffer(const std::vector<T>& vec, AllocatedBuffer& buffer, bool isVertexBuffer)
	{
		const size_t bufferSize{ vec.size() * sizeof(T) };
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

		// copy data
		void* data;
		vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
		std::memcpy(data, vec.data(), vec.size() * sizeof(T));
		vmaUnmapMemory(_allocator, stagingBuffer._allocation);

		VkBufferUsageFlags bufferUsage = isVertexBuffer ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		// now we need the GPU-side buffer since we've populated the staging buffer
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = bufferUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		// let the VMA library know that this data should be GPU native
		vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo,
			&buffer._buffer,
			&buffer._allocation,
			nullptr));

		immediateSubmit([=](VkCommandBuffer cmd) {
			VkBufferCopy copy;
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = bufferSize;
			vkCmdCopyBuffer(cmd, stagingBuffer._buffer, buffer._buffer, 1, &copy);
		});

		_mainDeletionQueue.pushFunction([=]() {
			vmaDestroyBuffer(_allocator, buffer._buffer, buffer._allocation);
		});
		vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation);
	}
};

class PipelineBuilder {
public:

	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;

	VkPipeline buildPipeline(VkDevice device, VkRenderPass pass, bool dynamicState);
};

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

#include "vk_types.h"
#include "vk_mem_alloc.h"
#include "vk_mesh.h"
#include "glm/glm.hpp"
#include "../tracy/Tracy.hpp"		// CPU profiling
#include "../tracy/TracyVulkan.hpp"
#include "application.h"
#include "physics.h"

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
constexpr uint32_t SHADOWMAP_DIM{ 2048 };
constexpr uint32_t MAX_OBJECTS{ 10000 };

struct VulkanEngine;

struct Light {
	glm::vec4 position; // w is unused
	glm::vec4 color;    // w is for intensity
};

struct DirectionLight {
	Light light;
	glm::vec4 direction;
};

struct Texture {
	AllocatedImage image;
	VkImageView imageView;
	uint32_t mipLevels;
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

struct GPUObjectData {
	glm::mat4 modelMatrix;
};

//struct OffscreenPass {
//	uint32_t width, height;
//	VkRenderPass renderPass;
//	std::array<VkFramebuffer, FRAME_OVERLAP> frameBuffers;
//	std::array <Texture, FRAME_OVERLAP> depths;
//	std::array <VkSampler, FRAME_OVERLAP> depthSamplers;
//	std::array <VkDescriptorImageInfo, FRAME_OVERLAP> descriptors;
//};

struct ShadowGlobalResources {
	uint32_t _width, _height;
	VkRenderPass _renderPass;
	// Depth bias (and slope) are used to avoid shadowing artifacts
	// Constant depth bias factor (always applied)
	float _depthBiasConstant{ 1.25f };
	// Slope depth bias factor, applied depending on polygon's slope
	float _depthBiasSlope{ 1.75f };
	VkPipeline _shadowPipeline;
	VkPipelineLayout _shadowPipelineLayout;
	glm::mat4 _lightSpaceMatrix;
};

struct ShadowFrameResources {
	VkFramebuffer frameBuffer;
	Texture depth;
	VkSampler depthSampler;
	VkDescriptorImageInfo descriptor;
	VkPipelineLayout _shadowPipelineLayout;
	VkDescriptorSet _shadowDescriptorSetLight;
	VkDescriptorSet _shadowDescriptorSetObjects;
	AllocatedBuffer _shadowLightBuffer;
};

struct FrameData {
	VkSemaphore _presentSemaphore;
	VkSemaphore _renderSemaphore;
	VkFence _renderFence;

	// This belongs to a frame because it's fast to reset a whole
	// command pool, and we reset this for the whole frame
	// (aka lifetime of this command pool is lifetime of this frame)
	VkCommandPool _commandPool;
	// command buffer belongs to frame since we for the next frame
	// while the other frame's command buffer is submitted
	VkCommandBuffer _mainCommandBuffer;

	// buffer that holds a single GPUCameraData to use when rendering
	AllocatedBuffer cameraBuffer;
	// descriptor that has frame lifetime
	VkDescriptorSet globalDescriptor;

	// Object matrices for all objects in scenes. This belongs to the frame since it's
	// dynamic and changing every frame (static scenes would need only one objectBuffer)
	AllocatedBuffer objectBuffer;
	VkDescriptorSet objectDescriptor;

	TracyVkCtx _tracyContext;

	ShadowFrameResources _shadow;
};

// note that we store the VkPipeline and layout by value, not pointer.
// They are 64 bit handles to internal driver structures anyway so storing
// a pointer to them isn't very useful
struct Material {
	// analogous to instance of descriptor set layout, which is why it's per material
	VkDescriptorSet textureSet;
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct MaterialCreateInfo {
	std::string name;
	std::string vertPath;
	std::string fragPath;
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	std::vector<Texture> bindingTextures;
};

struct RenderObject {
	Mesh* mesh;
	Material* material;
	mutable glm::mat4 transformMatrix;
	bool castShadow;

	bool operator<(const RenderObject& other) const;
};

struct Transform {
	glm::vec3 pos{ 0.0 };
	glm::vec3 scale{ 1.0 };
	glm::mat4 rot{ 1.0 };

	Transform();
	Transform(const PxTransform& pxt);
	glm::mat4 mat4();
	physx::PxTransform to_physx();
};

class GameObject {
public:
	GameObject(const RenderObject* ro)
		: _renderObject{ ro }
		, _transform{}
		, _physicsObject{}
	{}

	GameObject()
		: _renderObject{ nullptr }
		, _transform{}
		, _physicsObject{}
	{}

	void setRenderObject(const RenderObject* ro);

	physx::PxRigidActor* getPhysicsObject();

	void setPhysicsObject(physx::PxRigidActor* body);

	Transform getTransform();

	glm::vec3 getPos();

	glm::vec3 getScale();

	glm::mat4 getRot();


	void setTransform(Transform transform);

	void setPos(glm::vec3 pos);

	void setScale(glm::vec3 scale);

	void setRot(glm::mat4 rot);

	// physics

	void addForce(glm::vec3 force);

	void setVelocity(glm::vec3 velocity);

	void setMass(float mass);
	
	float getMass();

private:
	void updateRenderMatrix();

	Transform _transform;
	const RenderObject* _renderObject;
	physx::PxRigidActor* _physicsObject;
};

struct GuiData {
	Light light0;
	Light light1;
	float bedAngle;
	const RenderObject* bed;
	float roughness_mult;
};

struct MeshPushConstants {
	glm::vec4 roughness_multiplier; // only x component is used
	glm::mat4 render_matrix;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
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
	std::unordered_map<std::string, Mesh> _meshes;

	Transform _camTransform{};

	VkDescriptorSetLayout _globalSetLayout;
	VkDescriptorPool _descriptorPool;

	VkPhysicalDeviceProperties _gpuProperties;

	GPUSceneData _sceneParameters;
	AllocatedBuffer _sceneParameterBuffer;

	VkDescriptorSetLayout _objectSetLayout;

	UploadContext _uploadContext;

	//texture hashmap
	std::unordered_map<std::string, Texture> _loadedTextures;

	GuiData _guiData;

	// frame storage
	FrameData _frames[FRAME_OVERLAP];

	ShadowGlobalResources _shadowGlobal;

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

	// initializes everything in the engine
	void init(Application* app);

	// shuts down the engine
	void cleanup();

	// draw loop
	void draw();

	// run main loop
	void run();

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	void load_texture(const std::string& path, VkFormat format);

	bool load_shader_module(const std::string& filePath, VkShaderModule* outShaderModule);

	// create material and add it to the map
	Material* create_material(const MaterialCreateInfo& info, VkPipeline pipeline, VkPipelineLayout layout, VkDescriptorSetLayout materialSetLayout);

	// returns nullptr if it can't be found
	Mesh* get_mesh(const std::string& name);

	const RenderObject* create_render_object(const std::string& meshName, const std::string& matName, bool castShadow=true);

	const RenderObject* create_render_object(const std::string& name);

	void set_camera_transform(Transform transform);

	void set_scene_lights(const std::vector<Light>& lights);

	void add_to_physics_engine_dynamic(GameObject* go, PxShape* shape, float density = 10.0f);

	void add_to_physics_engine_dynamic_mass(GameObject* go, PxShape* shape, float mass);

	void add_to_physics_engine_static(GameObject* go, PxShape* shape);

	void update_physics();

	bool advance_physics(float delta);

	PxMaterial* create_physics_material(float staticFriciton, float dynamicFriction, float restitution);

	PxShape* create_physics_shape(const PxGeometry& geometry,
		const PxMaterial& material,
		bool isExclusive = false,
		PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE);

	void set_gravity(float gravity);

private:

	void init_vulkan();

	void init_swapchain();

	void init_commands();

	void init_default_renderpass();

	void init_framebuffers();

	void init_sync_structures();

	void init_pipeline(const MaterialCreateInfo& info, const std::string& prefix);

	void load_meshes();

	void load_mesh(const std::string& name, const std::string& path);

	void load_materials();

	void upload_mesh(Mesh& mesh);

	void showFPS();

	void init_scene();

	// getter for the frame we are rendering to right now
	FrameData& get_current_frame();

	// returns nullptr if it can't be found
	Material* get_material(const std::string& name);

	void draw_objects(VkCommandBuffer cmd, const std::multiset<RenderObject>& renderables);

	void init_descriptors();

	void init_object_buffers();

	void init_descriptor_pool();

	size_t pad_uniform_buffer_size(size_t originalSize);

	void init_imgui();

	void gui();

	void init_gui_data();

	std::vector<Texture> textures_from_binding_paths(const std::vector<std::string>& bindingPaths);

	void init_tracy();

	void shadow_pass(VkCommandBuffer& cmd);

	void init_shadow_pass();

	VkSampleCountFlagBits get_max_usable_sample_count(VkPhysicalDevice physicalDevice);
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

	VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};

#pragma once

#include <vector>
#include <string>
#include <limits>
#include <cstdint>
#include <unordered_map>

#include "vk_types.h"
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "json.hpp"

// Changing this value here also requires changing it in the vertex shader
constexpr uint32_t MAX_NUM_JOINTS{ 128 };

// ------------------------------------------------------------------------------------------ //
//                                         Vertex                                             //
// ------------------------------------------------------------------------------------------ //

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags{ 0 };
};

enum VertexAttributes {
	ATTR_POSITION = 1,
	ATTR_NORMAL = 2,
	ATTR_TANGENT = 4,
	ATTR_UV = 8,
	ATTR_JOINT_INDICES = 16,
	ATTR_JOINT_WEIGHTS = 32,
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec4 tangent; // w component is sign (from GLTF format)
	glm::vec2 uv;
};

struct VertexSkinned {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec4 tangent; // w component is sign (from GLTF format)
	glm::vec2 uv;
	glm::vec4 jointIndices;
	glm::vec4 jointWeights;
};

enum class VertexFormat : uint32_t
{
	Unknown = 0,
	DEFAULT,
	SKINNED,
};

VertexInputDescription getVertexDescription(uint32_t attrFlags, uint32_t stride);

// ------------------------------------------------------------------------------------------ //
//                                         Skeleton                                           //
// ------------------------------------------------------------------------------------------ //

struct Node;

struct Skin {
	std::string name;
	Node* skeletonRoot{}; // node which is root of skeleton
	//Node* meshNode{}; // node which has a pointer to the mesh
	std::vector<glm::mat4> inverseBindMatrices;
	std::vector<Node*> joints;
	AllocatedBuffer ubo; // pass actual joint matrices for current animation frame using ubo
	VkDescriptorSet jointsDescriptorSet;
	VkDescriptorSet jointsShadowDescriptorSet;
	VmaAllocator* allocator;

	struct UniformBlockSkinned {
		glm::mat4 jointMatrices[MAX_NUM_JOINTS]{};
		float jointCount{ 0 };
	} uniformBlock;

	void update(const glm::mat4& m);
};

enum class Interpolation {
	LINEAR,
	STEP,
	CUBICSPLINE
};

enum class NodeProperty {
	TRANSLATION,
	ROTATION,
	SCALE,
	WEIGHTS // for morph targets, not vertex weights
};

struct AnimationChannel {
	enum PathType { TRANSLATION, ROTATION, SCALE };
	PathType path;
	uint32_t nodeIdx;
	uint32_t samplerIndex;

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(path, nodeIdx, samplerIndex); // serialize things by passing them to the archive
	}
};

struct AnimationSampler {
	Interpolation interpolation;
	std::vector<float> inputs;
	std::vector<glm::vec4> outputsVec4;

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(interpolation, inputs, outputsVec4); // serialize things by passing them to the archive
	}
};

struct Animation {
	std::string name;
	std::vector<AnimationSampler> samplers;
	std::vector<AnimationChannel> channels;
	float start = std::numeric_limits<float>::max();
	float end = std::numeric_limits<float>::min();
	float currentTime{ 0.0f };

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(name, samplers, channels, start, end); // serialize things by passing them to the archive
	}
};

struct Mesh;
struct RenderObject;

struct Node {
	Node* parent;
	std::vector<Node*> children;
	glm::mat4 matrix;
	glm::mat4 cachedMatrix;
	std::string name;
	glm::vec3 translation{};
	glm::vec3 scale{ 1.0f };
	glm::quat rotation{};
	//BoundingBox bvh;
	//BoundingBox aabb;

	glm::mat4 localMatrix();
	glm::mat4 getMatrix();
	glm::mat4 getCachedMatrix();
	~Node();
};

struct SkeletalAnimationData {
	std::vector<Node> nodes;
	std::vector<Animation> animations;
	std::vector<Skin> skins;
	std::unordered_map<std::string, uint32_t> animNameToIndex;

	// Blender sets all interpolation to linear when "always sample animation" is enabled
	// so this option allows step interpolation even when you need to sample animation in Blender
	// (for example, when using bone constraints)
	bool forceStepInterpolation{ false };
};

// ------------------------------------------------------------------------------------------ //
//                                         Mesh                                               //
// ------------------------------------------------------------------------------------------ //

struct Mesh {
	VertexFormat vertexFormat;
	// vertex data on CPU
	std::vector<Vertex> vertices;
	std::vector<VertexSkinned> verticesSkinned;
	std::vector<uint16_t> indices;
	// vertex data on GPU
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer indexBuffer;

	SkeletalAnimationData skel;
};

struct MeshBounds {

	float origin[3];
	float radius;
	float extents[3];
};

// ------------------------------------------------------------------------------------------ //
//                                         Material                                           //
// ------------------------------------------------------------------------------------------ //

struct Texture {
	AllocatedImage image;
	VkImageView imageView;
	uint32_t mipLevels;
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
	uint32_t attributeFlags;
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	std::vector<Texture> bindingTextures;
};

struct RenderObject {
	Mesh* mesh;
	Material* material;
	mutable bool castShadow;
	mutable bool visible;
	mutable int32_t activeAnimation;

	struct RenderObjectUB {
		mutable glm::mat4 transformMatrix;
	} uniformBlock;

	bool operator<(const RenderObject& other) const;
	void updateSkin() const;
	void updateAnimation(float deltaTime) const;
	bool animated() const;
};

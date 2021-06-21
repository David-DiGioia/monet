#pragma once

#include <vector>
#include <string>
#include <limits>
#include <cstdint>

#include "vk_types.h"
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "asset_loader.h"
#include "json.hpp"

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
//                                         Mesh                                               //
// ------------------------------------------------------------------------------------------ //

struct AbstractMesh {
	VertexFormat vertexFormat;
	// vertex data on CPU
	std::vector<uint16_t> indices;
	// vertex data on GPU
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer indexBuffer;
};

template <typename V>
struct Mesh : public AbstractMesh {
	// vertex data on CPU
	std::vector<V> vertices;
};

struct MeshBounds {

	float origin[3];
	float radius;
	float extents[3];
};


// ------------------------------------------------------------------------------------------ //
//                                         Skeleton                                           //
// ------------------------------------------------------------------------------------------ //

struct Skin;

struct Node {
	Node* parent;
	uint32_t index;
	std::vector<Node*> children;
	glm::mat4 matrix;
	std::string name;
	Mesh<VertexSkinned>* mesh;
	Skin* skin;
	int32_t skinIndex = -1;
	glm::vec3 translation{};
	glm::vec3 scale{ 1.0f };
	glm::quat rotation{};
	//BoundingBox bvh;
	//BoundingBox aabb;

	glm::mat4 localMatrix();
	glm::mat4 getMatrix();
	void update();
	~Node();
};

struct Skin {
	std::string name;
	Node* skeletonRoot{ nullptr };
	std::vector<glm::mat4> inverseBindMatrices;
	std::vector<Node*> joints;
	AllocatedBuffer ssbo; // pass actual joint matrices for current animation frame using ssbo
	VkDescriptorSet descriptorSet;
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
	Node* node;
	uint32_t samplerIndex;
};

struct AnimationSampler {
	enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
	InterpolationType interpolation;
	std::vector<float> inputs;
	std::vector<glm::vec4> outputsVec4;
};

struct Animation {
	std::string name;
	std::vector<AnimationSampler> samplers;
	std::vector<AnimationChannel> channels;
	float start = std::numeric_limits<float>::max();
	float end = std::numeric_limits<float>::min();
};

struct SkeletalAnimationData {
	std::vector<Node*> nodes;
	std::vector<Node*> linearNodes;
	std::vector<Skin*> skins;
	std::vector<Animation> animations;
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
	AbstractMesh* mesh;
	Material* material;
	mutable glm::mat4 transformMatrix;
	bool castShadow;

	// Skinning
	std::vector<Node*> nodes;
	std::vector<Skin*> skins;
	std::vector<Animation> animations;

	bool operator<(const RenderObject& other) const;
};

// ------------------------------------------------------------------------------------------ //
//                                         Asset                                              //
// ------------------------------------------------------------------------------------------ //


namespace assets {

	struct MeshInfo {
		// size in bytes
		uint64_t vertexBufferSize;
		// size in bytes
		uint64_t indexBufferSize;
		MeshBounds bounds;
		VertexFormat vertexFormat;
		char indexSize;
		std::string originalFile;
		assets::CompressionMode compressionMode;
	};

	MeshInfo readMeshInfo(nlohmann::json& metadata);

	void unpackMesh(MeshInfo* info, const char* sourcebuffer, char* vertexBuffer, char* indexBuffer);

	assets::AssetFile packMesh(MeshInfo* info, char* vertexData, char* indexData);

	struct SkeletalAnimationInfo {
		// sizes in bytes
		uint32_t nodesSize;
		uint32_t linearNodesSize;
		uint32_t skinsSize;
		uint32_t animationsSize;

		std::string originalFile;
		assets::CompressionMode compressionMode;
	};

	void unpackSkeletalAnimation(SkeletalAnimationInfo* info, const char* sourcebuffer, char* nodeBuffer, char* skinBuffer, char* animationBuffer);

	assets::AssetFile packSkeletalAnimation(SkeletalAnimationInfo* info, char* nodeData, char* skinData, char* animationData);

	// Works for any vertex struct with position attribute
	template <typename T>
	MeshBounds calculateBounds(T* vertices, size_t count)
	{
		MeshBounds bounds;

		float min[3] = { std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max() };
		float max[3] = { std::numeric_limits<float>::min(),std::numeric_limits<float>::min(),std::numeric_limits<float>::min() };

		for (int i = 0; i < count; i++) {
			min[0] = std::min(min[0], vertices[i].position[0]);
			min[1] = std::min(min[1], vertices[i].position[1]);
			min[2] = std::min(min[2], vertices[i].position[2]);

			max[0] = std::max(max[0], vertices[i].position[0]);
			max[1] = std::max(max[1], vertices[i].position[1]);
			max[2] = std::max(max[2], vertices[i].position[2]);
		}

		bounds.extents[0] = (max[0] - min[0]) / 2.0f;
		bounds.extents[1] = (max[1] - min[1]) / 2.0f;
		bounds.extents[2] = (max[2] - min[2]) / 2.0f;

		bounds.origin[0] = bounds.extents[0] + min[0];
		bounds.origin[1] = bounds.extents[1] + min[1];
		bounds.origin[2] = bounds.extents[2] + min[2];

		//go through the vertices again to calculate the exact bounding sphere radius
		float r2 = 0;
		for (int i = 0; i < count; i++) {

			float offset[3];
			offset[0] = vertices[i].position[0] - bounds.origin[0];
			offset[1] = vertices[i].position[1] - bounds.origin[1];
			offset[2] = vertices[i].position[2] - bounds.origin[2];

			//pithagoras
			float distance = offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2];
			r2 = std::max(r2, distance);
		}

		bounds.radius = std::sqrt(r2);

		return bounds;
	}

}

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
#include "mesh_asset.h"

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

VertexInputDescription getVertexDescription(uint32_t attrFlags, uint32_t stride);

struct AbstractMesh {
	assets::VertexFormat vertexFormat;
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

struct Node {
	Node* parent;
	uint32_t index;
	std::vector<Node*> children;
	Mesh<VertexSkinned> mesh;
	glm::vec3 translation{};
	glm::vec3 scale{ 1.0f };
	glm::quat rotation{};
	int32_t skin{ -1 };
	glm::mat4 matrix;

	glm::mat4 getLocalMatrix();
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

struct AnimationSampler {
	Interpolation interpolation;
	std::vector<float> inputs;
	std::vector<glm::vec4> outputsVec4;
};

enum class NodeProperty {
	TRANSLATION,
	ROTATION,
	SCALE,
	WEIGHTS // for morph targets, not vertex weights
};

struct AnimationChannel {
	NodeProperty prop;
	Node* node;
	uint32_t samplerIndex;
};

struct Animation {
	std::string name;
	std::vector<AnimationSampler> samplers;
	std::vector<AnimationChannel> channels;
	float start{ std::numeric_limits<float>::max() };
	float end{ std::numeric_limits<float>::min() };
	float currentTime{ 0.0f };
};

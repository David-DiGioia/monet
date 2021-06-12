#pragma once

#include <vector>
#include <string>

#include "vk_types.h"
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags{ 0 };
};

enum VertexAttributes {
	ATTR_POSITION = 1,
	ATTR_NORMAL = 2,
	ATTR_TANGENT = 4,
	ATTR_UV = 8
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;

	static VertexInputDescription getVertexDescription(uint32_t attrFlags);
};

struct Mesh {
	// vertex data on CPU
	std::vector<Vertex> _vertices;
	std::vector<uint16_t> _indices;
	// vertex data on GPU
	AllocatedBuffer _vertexBuffer;
	AllocatedBuffer _indexBuffer;
};

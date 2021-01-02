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

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;

	static VertexInputDescription get_vertex_description();
};

struct Mesh {
	// vertex data on CPU
	std::vector<Vertex> _vertices;
	// vertex data on GPU
	AllocatedBuffer _vertexBuffer;

	bool load_from_obj(const std::string& filename);
};

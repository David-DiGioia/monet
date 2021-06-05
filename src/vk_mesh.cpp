#include "vk_mesh.h"

#include <cstddef> // offsetof
#include <iostream>

VertexInputDescription Vertex::getVertexDescription()
{
	VertexInputDescription description;

	// we will have just 1 vertex buffer binding, with a per-vertex rate
	VkVertexInputBindingDescription mainBinding{};
	mainBinding.binding = 0;
	mainBinding.stride = sizeof(Vertex);
	mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(mainBinding);

	VkVertexInputAttributeDescription positionAttribute{};
	positionAttribute.binding = 0;
	positionAttribute.location = 0;
	positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	positionAttribute.offset = offsetof(Vertex, position);

	VkVertexInputAttributeDescription normalAttribute{};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	normalAttribute.offset = offsetof(Vertex, normal);

	VkVertexInputAttributeDescription tangentAttribute{};
	tangentAttribute.binding = 0;
	tangentAttribute.location = 2;
	tangentAttribute.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	tangentAttribute.offset = offsetof(Vertex, tangent);

	VkVertexInputAttributeDescription uvAttribute{};
	uvAttribute.binding = 0;
	uvAttribute.location = 3;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT; // vec2
	uvAttribute.offset = offsetof(Vertex, uv);

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(tangentAttribute);
	description.attributes.push_back(uvAttribute);

	return description;
}

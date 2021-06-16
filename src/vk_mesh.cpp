#include "vk_mesh.h"

#include <cstddef> // offsetof
#include <iostream>


VertexInputDescription getVertexDescription(uint32_t attrFlags, uint32_t stride)
{
	VertexInputDescription description;

	// we will have just 1 vertex buffer binding, with a per-vertex rate
	VkVertexInputBindingDescription mainBinding{};
	mainBinding.binding = 0;
	mainBinding.stride = stride;
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
	tangentAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4
	tangentAttribute.offset = offsetof(Vertex, tangent);

	VkVertexInputAttributeDescription uvAttribute{};
	uvAttribute.binding = 0;
	uvAttribute.location = 3;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT; // vec2
	uvAttribute.offset = offsetof(Vertex, uv);

	VkVertexInputAttributeDescription jointIndicesAttribute{};
	jointIndicesAttribute.binding = 0;
	jointIndicesAttribute.location = 4;
	jointIndicesAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4
	jointIndicesAttribute.offset = offsetof(VertexSkinned, jointIndices);

	VkVertexInputAttributeDescription jointWeightsAttribute{};
	jointWeightsAttribute.binding = 0;
	jointWeightsAttribute.location = 5;
	jointWeightsAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4
	jointWeightsAttribute.offset = offsetof(VertexSkinned, jointWeights);

	if (attrFlags & ATTR_POSITION) description.attributes.push_back(positionAttribute);
	if (attrFlags & ATTR_NORMAL) description.attributes.push_back(normalAttribute);
	if (attrFlags & ATTR_TANGENT) description.attributes.push_back(tangentAttribute);
	if (attrFlags & ATTR_UV) description.attributes.push_back(uvAttribute);
	if (attrFlags & ATTR_JOINT_INDICES) description.attributes.push_back(jointIndicesAttribute);
	if (attrFlags & ATTR_JOINT_WEIGHTS) description.attributes.push_back(jointWeightsAttribute);

	return description;
}

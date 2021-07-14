#include "vk_mesh.h"

#include <cstddef> // offsetof
#include <iostream>
#include <algorithm>


bool RenderObject::operator<(const RenderObject& other) const
{
	if (material->pipeline == other.material->pipeline) {
		return mesh->vertexBuffer._buffer < other.mesh->vertexBuffer._buffer;
	} else {
		return material->pipeline < other.material->pipeline;
	}
}

void RenderObject::updateSkin() const
{
	for (Skin* skin : mesh->skel.skins) {
		skin->update(uniformBlock.transformMatrix);
	}
}

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
	positionAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4
	positionAttribute.offset = offsetof(Vertex, position);

	VkVertexInputAttributeDescription normalAttribute{};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4
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

void updateJointMatrices(Node* root, const glm::mat4& mat)
{
	root->cachedMatrix = root->localMatrix() * mat;

	for (Node* child : root->children) {
		updateJointMatrices(child, root->cachedMatrix);
	}
}

// m is the renderObject's transform.
// Updates skin's joint matrices, as well as each bone's transform.
// Also updates Skin's descriptor
void Skin::update(const glm::mat4& m)
{
	glm::mat4 inverseTransform = glm::inverse(m);
	size_t numJoints = (size_t)uniformBlock.jointCount;

	updateJointMatrices(skeletonRoot, glm::mat4(1.0f));

	for (size_t i = 0; i < numJoints; ++i) {
		glm::mat4 jointMat = joints[i]->getCachedMatrix() * inverseBindMatrices[i];
		jointMat = inverseTransform * jointMat;
		uniformBlock.jointMatrices[i] = jointMat;
	}

	void* data;
	vmaMapMemory(*allocator, ssbo._allocation, &data);
	memcpy(data, &uniformBlock, sizeof(UniformBlockSkinned));
	vmaUnmapMemory(*allocator, ssbo._allocation);
}

void Skin::updateAnimation(float deltaTime)
{

}

// Node

glm::mat4 Node::localMatrix()
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

// potentially expensive... try to avoid calling. instead call skin::update to update cached matrices, and access that
glm::mat4 Node::getMatrix()
{
	std::cout << "Node::getMatrix() called. Consider calling skin::update instead, then call Node:getCachedMatrix()\n";

	glm::mat4 m = localMatrix();
	Node* p{ parent };
	while (p) {
		m = p->localMatrix() * m;
		p = p->parent;
	}
	return m;
}

glm::mat4 Node::getCachedMatrix()
{
	return cachedMatrix;
}

Node::~Node()
{
	//if (renderObject) {
	//	delete renderObject;
	//}
	//for (auto& child : children) {
	//	delete child;
	//}
}

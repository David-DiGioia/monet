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
	for (Skin skin : mesh->skel.skins) {
		skin.update(uniformBlock.transformMatrix);
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
	root->cachedMatrix = mat * root->localMatrix();

	for (Node* child : root->children) {
		updateJointMatrices(child, root->cachedMatrix);
	}
}

// m is the renderObject's transform.
// Updates skin's joint matrices, as well as each bone's transform.
// Also updates Skin's descriptor
void Skin::update(const glm::mat4& m)
{
	//glm::mat4 inverseTransform = glm::inverse(m);
	size_t numJoints = (size_t)uniformBlock.jointCount;

	updateJointMatrices(skeletonRoot, glm::mat4(1.0f));

	for (size_t i = 0; i < numJoints; ++i) {
		glm::mat4 jointMat = joints[i]->getCachedMatrix() * inverseBindMatrices[i];
		//glm::mat4 jointMat = joints[i]->getMatrix() * inverseBindMatrices[i];
		//jointMat = inverseTransform * jointMat;
		uniformBlock.jointMatrices[i] = jointMat;
	}

	void* data;
	vmaMapMemory(*allocator, ubo._allocation, &data);
	memcpy(data, &uniformBlock, sizeof(UniformBlockSkinned));
	vmaUnmapMemory(*allocator, ubo._allocation);
	vmaFlushAllocation(*allocator, ubo._allocation, 0, VK_WHOLE_SIZE);
}

void RenderObject::updateAnimation(float deltaTime) const
{
	Animation& animation = mesh->skel.animations[activeAnimation];
	animation.currentTime += deltaTime;
	if (animation.currentTime > animation.end) {
		animation.currentTime -= animation.end;
	}

	for (AnimationChannel& channel : animation.channels) {

		AnimationSampler& sampler = animation.samplers[channel.samplerIndex];
		Node& node = mesh->skel.nodes[channel.nodeIdx];

		for (size_t i = 0; i < sampler.inputs.size() - 1; ++i) {

			// Get the input keyframe values for the current time stamp
			if ((animation.currentTime >= sampler.inputs[i]) && (animation.currentTime <= sampler.inputs[i + 1])) {

				float a{};

				if (sampler.interpolation == Interpolation::STEP || mesh->skel.forceStepInterpolation) {

					a = 0.0f;

				} else if (sampler.interpolation == Interpolation::LINEAR) {
					// Calculate interpolation value based on timestamp
					// at input1, a = 0, at input2 a=1, with linear interpolation
					a = (animation.currentTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
				}

				if (channel.path == AnimationChannel::TRANSLATION) {
					node.translation = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
				} else if (channel.path == AnimationChannel::ROTATION) {
					glm::quat q1;
					q1.x = sampler.outputsVec4[i].x;
					q1.y = sampler.outputsVec4[i].y;
					q1.z = sampler.outputsVec4[i].z;
					q1.w = sampler.outputsVec4[i].w;

					glm::quat q2;
					q2.x = sampler.outputsVec4[i + 1].x;
					q2.y = sampler.outputsVec4[i + 1].y;
					q2.z = sampler.outputsVec4[i + 1].z;
					q2.w = sampler.outputsVec4[i + 1].w;

					node.rotation = glm::normalize(glm::slerp(q1, q2, a));
				} else if (channel.path == AnimationChannel::SCALE) {
					node.scale = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
				}

			}
		}
	}

	updateSkin();
}

bool RenderObject::animated() const
{
	return !mesh->skel.animations.empty();
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

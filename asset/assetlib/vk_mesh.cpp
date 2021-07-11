#include "vk_mesh.h"

#include <cstddef> // offsetof
#include <iostream>
#include <algorithm>


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

VertexFormat parseFormat(const char* f) {

	if (strcmp(f, "DEFAULT") == 0) {
		return VertexFormat::DEFAULT;
	} else if (strcmp(f, "SKINNED") == 0) {
		return VertexFormat::SKINNED;
	} else {
		return VertexFormat::Unknown;
	}
}

assets::MeshInfo assets::readMeshInfo(nlohmann::json& metadata)
{
	assets::MeshInfo info;

	info.vertexBufferSize = metadata["vertex_buffer_size"];
	info.indexBufferSize = metadata["index_buffer_size"];
	info.indexSize = (uint8_t)metadata["index_size"];
	info.originalFile = metadata["original_file"];

	std::vector<float> boundsData;
	boundsData.reserve(7);
	boundsData = metadata["bounds"].get<std::vector<float>>();

	info.bounds.origin[0] = boundsData[0];
	info.bounds.origin[1] = boundsData[1];
	info.bounds.origin[2] = boundsData[2];

	info.bounds.radius = boundsData[3];

	info.bounds.extents[0] = boundsData[4];
	info.bounds.extents[1] = boundsData[5];
	info.bounds.extents[2] = boundsData[6];

	std::string vertexFormat = metadata["vertex_format"];
	info.vertexFormat = parseFormat(vertexFormat.c_str());

	std::string compressionMode = metadata["compression_mode"];
	info.compressionMode = assets::parseCompression(compressionMode.c_str());

	return info;
}

void assets::unpackMesh(MeshInfo* info, const char* sourcebuffer, char* vertexBuffer, char* indexBuffer)
{
	//copy vertex buffer
	memcpy(vertexBuffer, sourcebuffer, info->vertexBufferSize);

	//copy index buffer
	memcpy(indexBuffer, sourcebuffer + info->vertexBufferSize, info->indexBufferSize);
}

assets::AssetFile assets::packMesh(MeshInfo* info, char* vertexData, char* indexData)
{
	assets::AssetFile file;
	file.type[0] = 'M';
	file.type[1] = 'E';
	file.type[2] = 'S';
	file.type[3] = 'H';
	file.version = 1;

	size_t fullsize = info->vertexBufferSize + info->indexBufferSize;

	file.binaryBlob.resize(fullsize);

	// copy vertex buffer
	memcpy(file.binaryBlob.data(), vertexData, info->vertexBufferSize);

	// copy index buffer
	memcpy(file.binaryBlob.data() + info->vertexBufferSize, indexData, info->indexBufferSize);

	return file;
}

assets::SkeletalAnimationInfo assets::readSkeletalAnimationInfo(nlohmann::json& metadata)
{
	assets::SkeletalAnimationInfo info;

	info.nodesSize = metadata["nodes_size"];
	info.skinsSize = metadata["skins_size"];
	info.animationsSize = metadata["animations_size"];
	info.originalFile = metadata["original_file"];
	info.compressionMode = metadata["compression_mode"];

	return info;
}

void assets::unpackSkeletalAnimation(SkeletalAnimationInfo* info, const char* sourcebuffer, char* nodeBuffer, char* skinBuffer, char* animationBuffer)
{
	// copy node buffer
	memcpy(nodeBuffer, sourcebuffer, info->nodesSize);

	// copy skin buffer
	memcpy(skinBuffer, sourcebuffer + info->nodesSize, info->skinsSize);

	// copy animation buffer
	memcpy(animationBuffer, sourcebuffer + info->nodesSize + info->skinsSize, info->animationsSize);
}

assets::AssetFile assets::packSkeletalAnimation(SkeletalAnimationInfo* info, char* nodeData, char* skinData, char* animationData)
{
	assets::AssetFile file;
	file.type[0] = 'S';
	file.type[1] = 'K';
	file.type[2] = 'E';
	file.type[3] = 'L';
	file.version = 1;

	size_t fullsize = info->nodesSize + info->skinsSize + info->animationsSize;

	file.binaryBlob.resize(fullsize);

	memcpy(file.binaryBlob.data(), nodeData, info->nodesSize);
	memcpy(file.binaryBlob.data() + info->nodesSize, skinData, info->skinsSize);
	memcpy(file.binaryBlob.data() + info->nodesSize + info->skinsSize, animationData, info->animationsSize);

	return file;
}


void updateJointMatrices(Node* root, const glm::mat4& mat)
{
	root->cachedMatrix = root->localMatrix() * mat;

	for (Node* child : root->children) {
		updateJointMatrices(child, root->cachedMatrix);
	}
}

void Skin::update()
{
	glm::mat4 m = meshNode->getMatrix();

	meshNode->renderObject->uniformBlockSkinned->transformMatrix = m;
	glm::mat4 inverseTransform = glm::inverse(m);
	//size_t numJoints = std::min((uint32_t)joints.size(), MAX_NUM_JOINTS);
	size_t numJoints = (size_t)meshNode->renderObject->uniformBlockSkinned->jointCount;

	updateJointMatrices(skeletonRoot, glm::mat4(1.0f));

	for (size_t i = 0; i < numJoints; ++i) {
		glm::mat4 jointMat = joints[i]->getCachedMatrix() * inverseBindMatrices[i];
		jointMat = inverseTransform * jointMat;
		meshNode->renderObject->uniformBlockSkinned->jointMatrices[i] = jointMat;
	}

	//meshNode->renderObject->uniformBlockSkinned->jointCount = (float)numJoints;

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

// Update appropriate uniform block on CPU (without uploading to GPU) depending on whether skinned or not
/*
void Node::update()
{
	if (renderObject) {
		glm::mat4 m{ getMatrix() };

		if (skin) {

			renderObject->uniformBlockSkinned->transformMatrix = m;
			// Update join matrices
			glm::mat4 inverseTransform = glm::inverse(m);
			size_t numJoints = std::min((uint32_t)skin->joints.size(), MAX_NUM_JOINTS);

			for (size_t i = 0; i < numJoints; ++i) {
				Node* jointNode = skin->joints[i];
				glm::mat4 jointMat = jointNode->getMatrix() * skin->inverseBindMatrices[i];
				jointMat = inverseTransform * jointMat;
				renderObject->uniformBlockSkinned->jointMatrices[i] = jointMat;
			}

			renderObject->uniformBlockSkinned->jointCount = (float)numJoints;
			//memcpy(renderObject->uniformBuffer.mapped, &renderObject->uniformBlock, sizeof(renderObject->uniformBlock));

		} else {

			renderObject->uniformBlock->transformMatrix = m;
			//memcpy(renderObject->uniformBuffer.mapped, &m, sizeof(glm::mat4));

		}
	}

	for (auto& child : children) {
		child->update();
	}
}
*/

Node::~Node()
{
	if (renderObject) {
		delete renderObject;
	}
	for (auto& child : children) {
		delete child;
	}
}

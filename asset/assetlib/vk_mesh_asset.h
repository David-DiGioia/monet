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
#include "vk_mesh.h"

namespace assets {

	// Serializable version of node that we convert glTF files to before compressing and writing to disk
	struct NodeAsset {
		int32_t parentIdx;
		std::vector<int32_t> children;
		glm::mat4 matrix;
		std::string name;
		int32_t mesh;
		glm::vec3 translation{};
		glm::vec3 scale{ 1.0f };
		glm::quat rotation{};
		//BoundingBox bvh;
		//BoundingBox aabb;
	};

	struct SkinAsset {
		std::string name;
		int32_t skeletonRootIdx{}; // index of node which is root of skeleton
		int32_t meshNodeIdx{};
		std::vector<glm::mat4> inverseBindMatrices;
		std::vector<int32_t> joints; // indices of nodes
	};

	struct SkeletalAnimationDataAsset {
		std::vector<NodeAsset> nodes;
		std::vector<NodeAsset> linearNodes;
		std::vector<Animation> animations;
		std::vector<SkinAsset> skins;
	};

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

	SkeletalAnimationInfo readSkeletalAnimationInfo(nlohmann::json& metadata);

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

#pragma once
#include "asset_loader.h"

namespace assets {

	// custom, matches Vertex in vk_mesh.h
	struct Vertex_f32_PNTV {

		float position[3];
		float normal[3];
		float tangent[4];
		float uv[2];
	};

	// custom, matches VertexSkinned in vk_mesh.h
	struct Vertex_f32_PNTVIW {

		float position[3];
		float normal[3];
		float tangent[4]; // fourth component is sign
		float uv[2];
		float jointIndices[4];
		float jointWeights[4];
	};

	struct Vertex_f32_PNCV {

		float position[3];
		float normal[3];
		float color[3];
		float uv[2];
	};

	struct Vertex_P32N8C8V16 {

		float position[3];
		uint8_t normal[3];
		uint8_t color[3];
		float uv[2];
	};

	enum class VertexFormat : uint32_t
	{
		Unknown = 0,
		PNTV_F32,        //	tangent instead of color
		PNTVIW_F32,      //	skinned vertex
		PNCV_F32,        // everything at 32 bits
		P32N8C8V16       // position at 32 bits, normal at 8 bits, color at 8 bits, uvs at 16 bits float
	};

	struct MeshBounds {
		
		float origin[3];
		float radius;
		float extents[3];
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
		CompressionMode compressionMode;
	};

	MeshInfo readMeshInfo(nlohmann::json& metadata);

	void unpackMesh(MeshInfo* info, const char* sourcebuffer, char* vertexBuffer, char* indexBuffer);

	AssetFile packMesh(MeshInfo* info, char* vertexData, char* indexData);

	// Works for any vertex struct with position attribute
	template <typename T>
	assets::MeshBounds calculateBounds(T* vertices, size_t count)
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
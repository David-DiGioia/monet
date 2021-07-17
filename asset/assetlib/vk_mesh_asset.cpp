#include "vk_mesh_asset.h"

#include <cstddef> // offsetof
#include <iostream>
#include <algorithm>


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

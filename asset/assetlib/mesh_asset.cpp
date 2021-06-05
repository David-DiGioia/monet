#include "mesh_asset.h"
#include "json.hpp"


assets::VertexFormat parse_format(const char* f) {

	if (strcmp(f, "PNTV_F32") == 0) {
		return assets::VertexFormat::PNTV_F32;
	}
	else if (strcmp(f, "PNCV_F32") == 0) {
		return assets::VertexFormat::PNCV_F32;
	}
	else if (strcmp(f, "P32N8C8V16") == 0) {
		return assets::VertexFormat::P32N8C8V16;
	}
	else {
		return assets::VertexFormat::Unknown;
	}
}

assets::MeshInfo assets::readMeshInfo(AssetFile* file)
{
	MeshInfo info;

	nlohmann::json metadata = nlohmann::json::parse(file->json);

	
	info.vertexBufferSize = metadata["vertex_buffer_size"];		
	info.indexBufferSize = metadata["index_buffer_size"];
	info.indexSize = (uint8_t) metadata["index_size"];
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
	info.vertexFormat = parse_format(vertexFormat.c_str());
    return info;
}

void assets::unpackMesh(MeshInfo* info, const char* sourcebuffer, char* vertexBuffer, char* indexBuffer)
{
	//copy vertex buffer
	memcpy(vertexBuffer,sourcebuffer, info->vertexBufferSize);

	//copy index buffer
	memcpy(indexBuffer, sourcebuffer + info->vertexBufferSize, info->indexBufferSize);
}

assets::AssetFile assets::packMesh(MeshInfo* info, char* vertexData, char* indexData)
{
    AssetFile file;
	file.type[0] = 'M';
	file.type[1] = 'E';
	file.type[2] = 'S';
	file.type[3] = 'H';
	file.version = 1;

	nlohmann::json metadata;

	if (info->vertexFormat == VertexFormat::PNTV_F32) {
		metadata["vertex_format"] = "PNTV_F32";
	}
	else if (info->vertexFormat == VertexFormat::P32N8C8V16) {
		metadata["vertex_format"] = "P32N8C8V16";
	}
	else if (info->vertexFormat == VertexFormat::PNCV_F32) {
		metadata["vertex_format"] = "PNCV_F32";
	}

	metadata["vertex_buffer_size"] = info->vertexBufferSize;
	metadata["index_buffer_size"] = info->indexBufferSize;
	metadata["index_size"] = info->indexSize;
	metadata["original_file"] = info->originalFile;

	std::vector<float> boundsData;
	boundsData.resize(7);

	boundsData[0] = info->bounds.origin[0];
	boundsData[1] = info->bounds.origin[1];
	boundsData[2] = info->bounds.origin[2];

	boundsData[3] = info->bounds.radius;

	boundsData[4] = info->bounds.extents[0];
	boundsData[5] = info->bounds.extents[1];
	boundsData[6] = info->bounds.extents[2];

	metadata["bounds"] = boundsData;

	size_t fullsize = info->vertexBufferSize + info->indexBufferSize;

	file.binaryBlob.resize(fullsize);

	//copy vertex buffer
	memcpy(file.binaryBlob.data(), vertexData, info->vertexBufferSize);

	//copy index buffer
	memcpy(file.binaryBlob.data() + info->vertexBufferSize, indexData, info->indexBufferSize);

	file.json = metadata.dump();

	return file;
}

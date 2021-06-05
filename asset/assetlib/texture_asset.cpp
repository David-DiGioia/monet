#include "texture_asset.h"

#include <iostream>

#include "json.hpp"

assets::TextureFormat parse_format(const char* f) {

	if (strcmp(f, "RGBA8") == 0) {
		return assets::TextureFormat::RGBA8;
	} else if (strcmp(f, "SRGBA8") == 0) {
		return assets::TextureFormat::SRGBA8;
	} else {
		return assets::TextureFormat::Unknown;
	}
}

assets::TextureInfo assets::readTextureInfo(AssetFile* file)
{
	TextureInfo info;

	nlohmann::json texture_metadata = nlohmann::json::parse(file->json);

	std::string formatString = texture_metadata["format"];
	info.textureFormat = parse_format(formatString.c_str());
	info.textureSize = texture_metadata["buffer_size"];
	info.originalFile = texture_metadata["original_file"];
	info.miplevels = texture_metadata["miplevels"];
	info.width = texture_metadata["width"];
	info.height = texture_metadata["height"];

	return info;
}

void assets::unpackTexture(const char* sourcebuffer, size_t sourceSize, char* destination)
{
	memcpy(destination, sourcebuffer, sourceSize);
}


assets::AssetFile assets::packTexture(TextureInfo* info, void* pixelData)
{
	//core file header
	AssetFile file;
	file.type[0] = 'T';
	file.type[1] = 'E';
	file.type[2] = 'X';
	file.type[3] = 'I';
	file.version = 1;


	char* pixels = (char*)pixelData;

	file.binaryBlob.resize(info->textureSize);
	memcpy(file.binaryBlob.data(), pixels, info->textureSize);

	nlohmann::json texture_metadata;
	texture_metadata["format"] = "RGBA8";
	texture_metadata["buffer_size"] = info->textureSize;
	texture_metadata["original_file"] = info->originalFile;
	texture_metadata["miplevels"] = info->miplevels;
	texture_metadata["width"] = info->width;
	texture_metadata["height"] = info->height;

	std::string stringified = texture_metadata.dump();
	file.json = stringified;

	return file;
}


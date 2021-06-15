#include "texture_asset.h"

#include <iostream>

#include "json.hpp"
#include "lz4hc.h"
#include "lz4frame.h"

assets::TextureFormat parseFormat(const char* f) {

	if (strcmp(f, "RGBA8") == 0) {
		return assets::TextureFormat::RGBA8;
	} else if (strcmp(f, "SRGBA8") == 0) {
		return assets::TextureFormat::SRGBA8;
	} else {
		return assets::TextureFormat::Unknown;
	}
}

assets::TextureInfo assets::readTextureInfo(nlohmann::json& metadata)
{
	TextureInfo info;

	std::string formatString = metadata["format"];
	info.textureFormat = parseFormat(formatString.c_str());
	info.originalSize = metadata["original_size"];
	//info.compressedSize = texture_metadata["compressed_size"];
	info.originalFile = metadata["original_file"];
	info.miplevels = metadata["miplevels"];
	info.width = metadata["width"];
	info.height = metadata["height"];

	std::string compressionMode = metadata["compression_mode"];
	info.compressionMode = parseCompression(compressionMode.c_str());

	return info;
}

//void assets::unpackTexture(const char* compressedBuffer, char* destination, size_t compressedSize, size_t dstCapacity)
//{
//	LZ4F_dctx* context;
//	LZ4F_createDecompressionContext(&context, LZ4F_VERSION);
//
//	size_t bufHint{ LZ4F_decompress(context, destination, &dstCapacity, compressedBuffer, &compressedSize, nullptr) };
//	if (LZ4F_isError(bufHint)) {
//		std::cout << "Decompression failed\n";
//	}
//
//	std::cout << "compressedSize: " << compressedSize << "\n";
//
//	LZ4F_freeDecompressionContext(context);
//}

void assets::unpackTexture(const char* sourcebuffer, size_t sourceSize, void* destination)
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
	file.binaryBlob.resize(info->originalSize);
	memcpy(file.binaryBlob.data(), pixelData, file.binaryBlob.size());

	return file;
}


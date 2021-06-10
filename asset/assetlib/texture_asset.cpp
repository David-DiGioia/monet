#include "texture_asset.h"

#include <iostream>

#include "json.hpp"
#include "lz4hc.h"
#include "lz4frame.h"

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
	info.originalSize = texture_metadata["original_size"];
	info.compressedSize = texture_metadata["compressed_size"];
	info.originalFile = texture_metadata["original_file"];
	info.miplevels = texture_metadata["miplevels"];
	info.width = texture_metadata["width"];
	info.height = texture_metadata["height"];

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

	//LZ4F_preferences_t preferences{};
	//preferences.frameInfo.contentSize = info->originalSize;
	//preferences.compressionLevel = LZ4HC_CLEVEL_DEFAULT;
	//preferences.autoFlush = 1;
	//preferences.favorDecSpeed = 1;

	//// maximum size of compressed output
	//size_t maxDstSize{ LZ4F_compressFrameBound(info->originalSize, &preferences) };

	//// we will use that size for our destination boundary when allocating space
	//file.binaryBlob.resize(maxDstSize);

	//size_t compressedDataSize{ LZ4F_compressFrame(file.binaryBlob.data(), maxDstSize, (char*)pixelData, info->originalSize, &preferences) };
	//if (LZ4F_isError(compressedDataSize)) {
	//	std::cout << "Failed to compress data.\n";
	//} else {
	//	std::cout << "Compressed data with ratio " << (compressedDataSize / (float)info->originalSize) << "\n\n";
	//}

	//info->compressedSize = compressedDataSize;
	//file.binaryBlob.resize(compressedDataSize);

	nlohmann::json texture_metadata;
	texture_metadata["format"] = "RGBA8";
	texture_metadata["original_size"] = info->originalSize;
	texture_metadata["compressed_size"] = info->compressedSize;
	texture_metadata["original_file"] = info->originalFile;
	texture_metadata["miplevels"] = info->miplevels;
	texture_metadata["width"] = info->width;
	texture_metadata["height"] = info->height;

	std::string stringified = texture_metadata.dump();
	file.json = stringified;

	return file;
}


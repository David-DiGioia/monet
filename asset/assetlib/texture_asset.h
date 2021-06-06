#pragma once
#include "asset_loader.h"

namespace assets {

	enum class TextureFormat : uint32_t
	{
		Unknown = 0,
		RGBA8,
		SRGBA8
	};

	struct TextureInfo {
		// size in bytes
		uint64_t originalSize;
		uint64_t compressedSize;
		// width and height are in texels
		uint32_t width;
		uint32_t height;
		TextureFormat textureFormat;
		std::string originalFile;
		uint32_t miplevels;
	};

	TextureInfo readTextureInfo(AssetFile* file);

	void unpackTexture(const char* compressedBuffer, char* destination, size_t compressedSize, size_t dstCapacity);

	AssetFile packTexture(TextureInfo* info, void* pixelData);
}
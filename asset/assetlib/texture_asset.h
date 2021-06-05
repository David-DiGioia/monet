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
		uint64_t textureSize;
		// width and height are in texels
		uint32_t width;
		uint32_t height;
		TextureFormat textureFormat;
		std::string originalFile;
		uint32_t miplevels;
	};

	TextureInfo read_texture_info(AssetFile* file);

	void unpack_texture(const char* sourcebuffer, size_t sourceSize, char* destination);

	AssetFile pack_texture(TextureInfo* info, void* pixelData);
}
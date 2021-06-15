#pragma once
#include <vector>
#include <string>

#include "json.hpp"

namespace assets {
	struct AssetFile {
		char type[4];
		int version;
		std::string json;
		std::vector<char> binaryBlob;
	};

	enum class CompressionMode : uint32_t {
		None,
		LZ4
	};

	// Writes metadata to file.json
	bool saveBinaryFile(const char* path, nlohmann::json& metadata, AssetFile& file);

	bool loadBinaryFile(const char* path, AssetFile& asset, nlohmann::json& metadataOut);

	assets::CompressionMode parseCompression(const char* f);
}

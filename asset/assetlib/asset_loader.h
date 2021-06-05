#pragma once
#include <vector>
#include <string>

namespace assets {
	struct AssetFile {
		char type[4];
		int version;
		std::string json;
		std::vector<char> binaryBlob;
	};

	bool saveBinaryFile(const char* path, const AssetFile& file);

	bool loadBinaryFile(const char* path, AssetFile& outputFile);	
}

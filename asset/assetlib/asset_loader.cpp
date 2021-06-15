#include "asset_loader.h"

#include <fstream>
#include <iostream>

#include "../tracy/Tracy.hpp"		// CPU profiling
#include "compression.h"

using namespace assets;

bool assets::saveBinaryFile(const char* path, nlohmann::json& metadata, AssetFile& file)
{
	//pixel data
	char* compressedBlob{ new char[file.json.size() + file.binaryBlob.size()] };

	CompressResult_t res{ compressBuffer(file.binaryBlob.data(), compressedBlob, file.binaryBlob.size()) };

	float compressionRatio{ (float)res.sizeOut / (float)res.sizeIn };
	float thresholdRatio{ 0.8f };
	bool useCompression{ compressionRatio < thresholdRatio };
	useCompression = false;

	if (useCompression) {
		std::cout << "Compression ratio (" << compressionRatio << ") < threshold (" << thresholdRatio << "), compressing binary blob\n\n";
		metadata["compression_mode"] = "LZ4";
	} else {
		std::cout << "Compression ratio (" << compressionRatio << ") >= threshold (" << thresholdRatio << "), NOT compressing binary blob\n\n";
		metadata["compression_mode"] = "None";
	}
	file.json = metadata.dump();


	std::ofstream outFile;
	outFile.open(path, std::ios::binary | std::ios::out);
	if (!outFile.is_open()) {
		std::cout << "Error when trying to write file: " << path << std::endl;
	}
	outFile.write(file.type, 4);
	uint32_t version = file.version;
	//version
	outFile.write((const char*)&version, sizeof(uint32_t));

	//json length
	uint32_t length = static_cast<uint32_t>(file.json.size());
	outFile.write((const char*)&length, sizeof(uint32_t));

	//blob length
	uint32_t blobLength = static_cast<uint32_t>(file.binaryBlob.size());
	outFile.write((const char*)&blobLength, sizeof(uint32_t));

	//json stream
	outFile.write(file.json.data(), length);


	if (useCompression) {
		outFile.write(compressedBlob, res.sizeOut);
	} else {
		outFile.write(file.binaryBlob.data(), file.binaryBlob.size());
	}

	delete[] compressedBlob;

	outFile.close();

	return true;
}

bool assets::loadBinaryFile(const char* path, AssetFile& asset, nlohmann::json& metadataOut)
{
	std::ifstream inFile;
	{
		ZoneScopedN("open file");
		inFile.open(path, std::ios::binary);
	}

	if (!inFile.is_open()) return false;

	inFile.seekg(0);
	uint32_t bloblen = 0;
	{
		ZoneScopedN("read header");
		inFile.read(asset.type, 4);

		inFile.read((char*)&asset.version, sizeof(uint32_t));

		uint32_t jsonlen = 0;
		inFile.read((char*)&jsonlen, sizeof(uint32_t));

		inFile.read((char*)&bloblen, sizeof(uint32_t));

		asset.json.resize(jsonlen);

		inFile.read(asset.json.data(), jsonlen);

		asset.binaryBlob.resize(bloblen);
	}

	metadataOut = nlohmann::json::parse(asset.json);
	std::string compressionModeString = metadataOut["compression_mode"];
	CompressionMode compressionMode{ parseCompression(compressionModeString.c_str()) };

	if (compressionMode == CompressionMode::LZ4) {
		ZoneScopedN("decompress file");
		decompressFile(inFile, asset.binaryBlob.data());
	} else {
		inFile.read(asset.binaryBlob.data(), bloblen);
	}

	return true;
}

assets::CompressionMode assets::parseCompression(const char* f)
{
	if (strcmp(f, "LZ4") == 0) {
		return assets::CompressionMode::LZ4;
	} else {
		return assets::CompressionMode::None;
	}
}

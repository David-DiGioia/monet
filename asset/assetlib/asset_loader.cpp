#include "asset_loader.h"

#include <fstream>
#include <iostream>

#define TRACY_ENABLE
#include "../tracy/Tracy.hpp"		// CPU profiling
#include "compression.h"

using namespace assets;

bool assets::saveBinaryFile(const char* path, const AssetFile& file)
{
	std::ofstream outFile;
	outFile.open(path, std::ios::binary | std::ios::out);
	if (!outFile.is_open())
	{
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

	//pixel data
	char* compressedBlob{ new char[file.binaryBlob.size()] };

	CompressResult_t res{ compressBuffer(file.binaryBlob.data(), compressedBlob, file.binaryBlob.size()) };
	//outFile.write(file.binaryBlob.data(), file.binaryBlob.size());
	outFile.write(compressedBlob, res.sizeOut);

	delete[] compressedBlob;

	outFile.close();

	return true;
}

bool assets::loadBinaryFile(const char* path, AssetFile& outputFile)
{
	std::ifstream inFile;
	{
		ZoneScopedN("open file");
		inFile.open(path, std::ios::binary);
	}

	if (!inFile.is_open()) return false;

	inFile.seekg(0);

	{
		ZoneScopedN("read header");
		inFile.read(outputFile.type, 4);

		inFile.read((char*)&outputFile.version, sizeof(uint32_t));

		uint32_t jsonlen = 0;
		inFile.read((char*)&jsonlen, sizeof(uint32_t));

		uint32_t bloblen = 0;
		inFile.read((char*)&bloblen, sizeof(uint32_t));

		outputFile.json.resize(jsonlen);

		inFile.read(outputFile.json.data(), jsonlen);

		outputFile.binaryBlob.resize(bloblen);
	}
	{
		ZoneScopedN("decompress file");
		//inFile.read(outputFile.binaryBlob.data(), bloblen);
		decompressFile(inFile, outputFile.binaryBlob.data());
	}
	return true;
}

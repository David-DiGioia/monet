#include "asset_loader.h"

#include <fstream>
#include <iostream>

using namespace assets;

bool assets::saveBinaryFile(const  char* path, const AssetFile& file)
{
	std::ofstream outfile;
	outfile.open(path, std::ios::binary | std::ios::out);
	if (!outfile.is_open())
	{
		std::cout << "Error when trying to write file: " << path << std::endl;
	}
	outfile.write(file.type, 4);
	uint32_t version = file.version;
	//version
	outfile.write((const char*)&version, sizeof(uint32_t));

	//json length
	uint32_t length = static_cast<uint32_t>(file.json.size());
	outfile.write((const char*)&length, sizeof(uint32_t));

	//blob length
	uint32_t blobLength = static_cast<uint32_t>(file.binaryBlob.size());
	outfile.write((const char*)&blobLength, sizeof(uint32_t));

	//json stream
	outfile.write(file.json.data(), length);
	//pixel data
	outfile.write(file.binaryBlob.data(), file.binaryBlob.size());

	outfile.close();

	return true;
}

bool assets::loadBinaryFile(const  char* path, AssetFile& outputFile)
{
	std::ifstream infile;
	infile.open(path, std::ios::binary);

	if (!infile.is_open()) return false;

	infile.seekg(0);


	infile.read(outputFile.type, 4);
	
	infile.read((char*)&outputFile.version, sizeof(uint32_t));

	uint32_t jsonlen = 0;
	infile.read((char*)&jsonlen, sizeof(uint32_t));

	uint32_t bloblen = 0;
	infile.read((char*)&bloblen, sizeof(uint32_t));

	outputFile.json.resize(jsonlen);

	infile.read(outputFile.json.data(), jsonlen);

	outputFile.binaryBlob.resize(bloblen);
	infile.read(outputFile.binaryBlob.data(), bloblen);

	return true;
}

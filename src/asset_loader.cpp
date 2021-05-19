#include "asset_loader.h"

#include <fstream>

bool assets::save_binaryfile(const char* path, const assets::AssetFile& file)
{
	std::ofstream outfile;
	outfile.open(path, std::ios::binary | std::ios::out);

	outfile.write(file.type, 4);

	// version
	uint32_t version = file.version;
	outfile.write((const char*)&version, sizeof(uint32_t));

	// json length
	uint32_t length = file.json.size();
	outfile.write((const char*)&length, sizeof(uint32_t));

	// blob length
	uint32_t blobLength = file.binaryBlob.size();
	outfile.write((const char*)&blobLength, sizeof(uint32_t));

	// json stream
	outfile.write(file.json.data(), length);

	//blob data
	outfile.write(file.binaryBlob.data(), file.binaryBlob.size());

	outfile.close();

	return true;
}

bool assets::load_binaryfile(const char* path, assets::AssetFile& outputFile)
{
	std::ifstream infile;
	infile.open(path, std::ios::binary);

	if (!infile.is_open()) return false;

	// move file cursor to begining
	infile.seekg(0);

	infile.read(outputFile.type, 4);
	infile.read((char*)&outputFile.version, sizeof(uint32_t));

	uint32_t jsonlen{ 0 };
	infile.read((char*)&jsonlen, sizeof(uint32_t));

	uint32_t bloblen{ 0 };
	infile.read((char*)&bloblen, sizeof(uint32_t));

	outputFile.json.resize(jsonlen);

	infile.read(outputFile.json.data(), jsonlen);

	outputFile.binaryBlob.resize(bloblen);
	infile.read(outputFile.binaryBlob.data(), bloblen);

	return true;
}

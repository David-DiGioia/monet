#include <cinttypes>
#include <fstream>

struct CompressResult_t {
	int error;
	unsigned long long sizeIn;
	unsigned long long sizeOut;
};

// Compress a buffer using LZ4
CompressResult_t compressBuffer(const void* inBuf, void* outBuf, uint32_t inBufSize);

// Stream decompress file using LZ4
int decompressFile(std::ifstream& inFile, void* outBuf);

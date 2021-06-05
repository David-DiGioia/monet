#pragma once

#include "vk_types.h"
#include "vk_engine.h"

namespace vkutil {

	bool loadImageFromAsset(VulkanEngine& engine, const char* filename, VkFormat format, uint32_t* outMipLevels, AllocatedImage& outImage);

	bool loadImageFromFile(VulkanEngine& engine, const char* file, AllocatedImage& outImage, uint32_t* outMipLevels, VkFormat format);

	void generateMipmaps(VkCommandBuffer cmd, VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	uint32_t getMipLevels(uint32_t width, uint32_t height);

	VkExtent2D nextMipLevelExtent(VkExtent2D extent);
}
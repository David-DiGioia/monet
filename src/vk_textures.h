#pragma once

#include "vk_types.h"
#include "vk_engine.h"

namespace vkutil {

	bool load_image_from_file(VulkanEngine& engine, const char* file, AllocatedImage& outImage, uint32_t* outMipLevels, VkFormat format, bool okToFail = false);

	void generateMipmaps(VkCommandBuffer cmd, VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

}
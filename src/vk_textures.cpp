#include "vk_textures.h"

#include <iostream>

#include "vk_initializers.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool vkutil::load_image_from_file(VulkanEngine& engine, const char* file, AllocatedImage& outImage, bool okToFail)
{
	int texWidth, texHeight, texChannels;

	stbi_uc* pixels{ stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha) };

	if (!pixels) {
		if (okToFail) {
			std::cout << "Using default texture instead of '" << file << "'\n";
		} else {
			std::cout << "Error: Failed to load texture file '" << file << "'\n";
		}
		return false;
	}

	void* pixel_ptr{ pixels };
	VkDeviceSize imageSize{ static_cast<VkDeviceSize>(texWidth * texHeight * 4) };

	// the format R8G8B8A8 matches exactly with the pixels loaded from stb_image library
	VkFormat image_format{ VK_FORMAT_R8G8B8A8_UNORM };

	// allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer{ engine.create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY) };

	void* data;
	vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);
	std::memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
	vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);
	// pixels was copied to staging buffer so we free it
	stbi_image_free(pixels);

	VkExtent3D imageExtent{};
	imageExtent.width = static_cast<uint32_t>(texWidth);
	imageExtent.height = static_cast<uint32_t>(texHeight);
	imageExtent.depth = 1;

	VkImageCreateInfo dimg_info{ vkinit::image_create_info(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent) };

	AllocatedImage newImage;

	VmaAllocationCreateInfo dimg_allocInfo{};
	dimg_allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	// allocate and create the image
	vmaCreateImage(engine._allocator, &dimg_info, &dimg_allocInfo,
		&newImage._image,
		&newImage._allocation,
		nullptr);

	// we must transfer the image to linear layout before copying the buffer to the image
	engine.immediate_submit([=](VkCommandBuffer cmd) {
		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier imageBarrierToTransfer{};
		imageBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrierToTransfer.pNext = nullptr;
		imageBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrierToTransfer.image = newImage._image;
		imageBarrierToTransfer.subresourceRange = range;
		imageBarrierToTransfer.srcAccessMask = 0;
		imageBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrierToTransfer);

		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = imageExtent;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, stagingBuffer._buffer, newImage._image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// now that we've copied the buffer into the image, we change the
		// image layout once more to make it readable from shaders
		VkImageMemoryBarrier imageBarrierToReadable{ imageBarrierToTransfer };
		imageBarrierToReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrierToReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrierToReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrierToReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrierToReadable);
	});

	engine._mainDeletionQueue.push_function([=]() {
		vmaDestroyImage(engine._allocator, newImage._image, newImage._allocation);
	});

	vmaDestroyBuffer(engine._allocator, stagingBuffer._buffer, stagingBuffer._allocation);

	std::cout << "Texture loaded successfully " << file << '\n';

	outImage = newImage;
	return true;
}

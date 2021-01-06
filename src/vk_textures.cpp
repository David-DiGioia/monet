#include "vk_textures.h"

#include <iostream>
#include <cmath>

#include "vk_initializers.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// expects all mip layers to currently be in TRANSFER_DST layout and
// will leave it in TRANSFER_SRC layout when the function returns
void vkutil::generateMipmaps(VkCommandBuffer cmd, VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// This part of the barrier is the same for all barriers
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth{ texWidth };
	int32_t mipHeight{ texHeight };

	for (auto i{ 1 }; i < mipLevels; ++i) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(cmd,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	// now we transition the last mip level's layout to TRANSFER_SRC so that each mip level matches, making it easier to deal with later
	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
}

bool vkutil::load_image_from_file(VulkanEngine& engine, const char* file, AllocatedImage& outImage, uint32_t* outMipLevels, VkFormat format)
{
	int texWidth, texHeight, texChannels;
	uint32_t pixelBytes{ 4 };
	void* pixel_ptr{ nullptr };

	// if we're loading an hdri we handle the special case
	if (format == VK_FORMAT_R32G32B32A32_SFLOAT) {
		stbi_set_flip_vertically_on_load(true);
		float* pixels{ stbi_loadf(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha) };
		pixelBytes = 16;
		pixel_ptr = pixels;
	} else {
		stbi_set_flip_vertically_on_load(false);
		stbi_uc* pixels{ stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha) };
		pixel_ptr = pixels;
	}

	if (!pixel_ptr) {
		return false;
	}


	VkDeviceSize imageSize{ static_cast<VkDeviceSize>(texWidth * texHeight * pixelBytes) };

	// allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer{ engine.create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY) };

	void* data;
	vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);
	std::memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
	vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);
	// pixels was copied to staging buffer so we free it
	stbi_image_free(pixel_ptr);

	VkExtent3D imageExtent{};
	imageExtent.width = static_cast<uint32_t>(texWidth);
	imageExtent.height = static_cast<uint32_t>(texHeight);
	imageExtent.depth = 1;
	uint32_t mipLevels{ static_cast<uint32_t>(std::floor(std::log2((std::max(texWidth, texHeight))))) + 1 };
	*outMipLevels = mipLevels;

	VkImageCreateInfo dimg_info{ vkinit::image_create_info(format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageExtent, mipLevels) };

	AllocatedImage newImage;

	VmaAllocationCreateInfo dimg_allocInfo{};
	dimg_allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	// allocate and create the image
	vmaCreateImage(engine._allocator, &dimg_info, &dimg_allocInfo,
		&newImage._image,
		&newImage._allocation,
		nullptr);

	// we must transfer the image to transfer dst layout before copying the buffer to the image
	engine.immediate_submit([=](VkCommandBuffer cmd) {
		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = mipLevels;
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

		vkutil::generateMipmaps(cmd, newImage._image, texWidth, texHeight, mipLevels);

		// now that we've copied the buffer into the image, we change the
		// image layout once more to make it readable from shaders
		VkImageMemoryBarrier imageBarrierToReadable{ imageBarrierToTransfer };
		imageBarrierToReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // src since generateMipmaps transitioned each mip level to transfer src
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

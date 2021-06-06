#include "vk_textures.h"

#include <iostream>
#include <cmath>

#include "vk_initializers.h"
#include "asset_loader.h"
#include "texture_asset.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

uint32_t vkutil::getMipLevels(uint32_t width, uint32_t height)
{
	return static_cast<uint32_t>(std::floor(std::log2((std::max(width, height))))) + 1;
}

VkExtent2D vkutil::nextMipLevelExtent(VkExtent2D extent)
{
	extent.width = extent.width > 1 ? extent.width / 2 : 1;
	extent.height = extent.height > 1 ? extent.height / 2 : 1;
	return extent;
}

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

void upload_image(VulkanEngine& engine, assets::TextureInfo info, VkFormat format, const AllocatedBuffer& stagingBuffer, AllocatedImage& outImage) {
	ZoneScoped
	VkExtent3D imageExtent{};
	imageExtent.width = static_cast<uint32_t>(info.width);
	imageExtent.height = static_cast<uint32_t>(info.height);
	imageExtent.depth = 1;

	VkImageCreateInfo dimg_info{ vkinit::imageCreateInfo(format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageExtent, info.miplevels) };

	AllocatedImage newImage;

	VmaAllocationCreateInfo dimg_allocInfo{};
	dimg_allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	// allocate and create the image
	vmaCreateImage(engine._allocator, &dimg_info, &dimg_allocInfo,
		&newImage._image,
		&newImage._allocation,
		nullptr);

	// we must transfer the image to transfer dst layout before copying the buffer to the image
	engine.immediateSubmit([=](VkCommandBuffer cmd) {
		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = info.miplevels;
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

		VkExtent3D extent{ imageExtent };
		VkDeviceSize offset{ 0 };
		std::vector<VkBufferImageCopy> copyRegions;

		for (int i{ 0 }; i < info.miplevels; ++i) {
			VkBufferImageCopy copyRegion{};
			copyRegion.bufferOffset = offset * 4; // multiply by 4 since texel is 4 bytes
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = i;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent = extent;

			copyRegions.push_back(copyRegion);

			// halve dimensions of image for each mipmap level
			offset += (VkDeviceSize)extent.width * extent.height;
			extent.width >>= 1;
			extent.height >>= 1;
		}

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, stagingBuffer._buffer, newImage._image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegions.size(), copyRegions.data());

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

	engine._mainDeletionQueue.pushFunction([=, &engine]() {
		vmaDestroyImage(engine._allocator, newImage._image, newImage._allocation);
	});

	// caller responsible for destroying staging buffer

	outImage = newImage;
}

bool vkutil::loadImageFromAsset(VulkanEngine& engine, const char* path, VkFormat format, uint32_t* outMipLevels, AllocatedImage& outImage)
{
	ZoneScoped
	assets::AssetFile file;

	{
		ZoneScopedN("load_binaryfile")
		bool loaded{ assets::loadBinaryFile(path, file) };

		if (!loaded) {
			std::cout << "Error when loading image\n";
			return false;
		}
	}

	assets::TextureInfo texInfo;

	{
		ZoneScopedN("read_texture_info")
		texInfo = assets::readTextureInfo(&file);
	}

	*outMipLevels = (uint32_t)texInfo.miplevels;

	VkFormat image_format;
	switch (texInfo.textureFormat) {

	case assets::TextureFormat::RGBA8:
		image_format = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	case assets::TextureFormat::SRGBA8:
		image_format = VK_FORMAT_R8G8B8A8_SRGB;
		break;
	default:
		return false;
	}

	AllocatedBuffer stagingBuffer{ engine.createBuffer(texInfo.originalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY) };

	void* data;
	vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);

	{
		ZoneScopedN("unpack_texture")
		assets::unpackTexture(file.binaryBlob.data(), (char*)data, texInfo.compressedSize, texInfo.originalSize);
	}

	vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);

	//outImage = upload_image(textureInfo.pixelsize[0], textureInfo.pixelsize[1], image_format, engine, stagingBuffer);
	upload_image(engine, texInfo, format, stagingBuffer, outImage);

	vmaDestroyBuffer(engine._allocator, stagingBuffer._buffer, stagingBuffer._allocation);

	std::cout << "Texture loaded successfully " << path << '\n';

	return true;
}

bool vkutil::loadImageFromFile(VulkanEngine& engine, const char* file, AllocatedImage& outImage, uint32_t* outMipLevels, VkFormat format)
{
	int texWidth, texHeight, texChannels;
	uint32_t pixelBytes{ 4 };
	void* pixel_ptr{ nullptr };

	bool hdri{ format == VK_FORMAT_R32G32B32A32_SFLOAT };

	// if we're loading an hdri we handle the special case
	if (hdri) {
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
	AllocatedBuffer stagingBuffer{ engine.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY) };

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
	// HDRIs should not be mipmapped
	uint32_t mipLevels{ hdri ? 1 : getMipLevels(texWidth, texHeight) };
	*outMipLevels = mipLevels;

	VkImageCreateInfo dimg_info{ vkinit::imageCreateInfo(format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageExtent, mipLevels) };

	AllocatedImage newImage;

	VmaAllocationCreateInfo dimg_allocInfo{};
	dimg_allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	// allocate and create the image
	vmaCreateImage(engine._allocator, &dimg_info, &dimg_allocInfo,
		&newImage._image,
		&newImage._allocation,
		nullptr);

	// we must transfer the image to transfer dst layout before copying the buffer to the image
	engine.immediateSubmit([=](VkCommandBuffer cmd) {
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

	engine._mainDeletionQueue.pushFunction([=, &engine]() {
		vmaDestroyImage(engine._allocator, newImage._image, newImage._allocation);
	});

	vmaDestroyBuffer(engine._allocator, stagingBuffer._buffer, stagingBuffer._allocation);

	std::cout << "Texture loaded successfully " << file << '\n';

	outImage = newImage;
	return true;
}

// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#pragma warning(disable : 26812) // The enum type * is unscoped. Prefer 'enum class' over 'enum'.

#include "vk_types.h"

namespace vkinit {

	VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule);

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo(VkPrimitiveTopology topology);

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo(VkPolygonMode polygonMode);

	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo(VkSampleCountFlagBits msaaSamples, float minSampleShading);

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState();

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();

	VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, uint32_t mipLevels, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);

	VkImageViewCreateInfo imageviewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);

	VkDescriptorSetLayoutBinding descriptorsetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);

	VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

	VkSamplerCreateInfo samplerCreateInfo(VkFilter filters, uint32_t mipLevels, VkSamplerAddressMode samplerAddressMode);

	VkWriteDescriptorSet writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);
}

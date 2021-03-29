#pragma once

#include "vk_types.h"
#include "vk_engine.h"

// return texture with 6 layers, representing each face of cube
Texture render_to_texture(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent, bool useMipmap, bool isCubemap, const std::string& vertPath, const std::string& fragPath);

void prepareShadowMapFramebuffer(VulkanEngine& engine, OffscreenPass* offscreenPass);

void initShadowPipeline(VulkanEngine& engine, OffscreenPass& offscreenPass, VkPipelineLayout pipelineLayout, VkPipeline* pipeline);

void setupDescriptorSetLayouts(VulkanEngine& engine, std::array<VkDescriptorSetLayout, 2>& setLayoutsOut, VkPipelineLayout* pipelineLayout);

void setupDescriptorSets(VulkanEngine& engine, std::array<VkDescriptorSetLayout, 2>& setLayouts);

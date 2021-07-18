#pragma once

#include "vk_types.h"
#include "vk_engine.h"

// return texture with 6 layers, representing each face of cube
Texture renderToTexture(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent, bool useMipmap, bool isCubemap, const std::string& vertPath, const std::string& fragPath);

void prepareShadowMapFramebuffer(VulkanEngine& engine, const ShadowGlobalResources& shadowGlobal, ShadowFrameResources* shadowFrame);

void prepareShadowMapRenderpass(VulkanEngine& engine, VkRenderPass* renderpass);

void initShadowPipeline(VulkanEngine& engine, VkRenderPass& renderpass, VkPipelineLayout pipelineLayout, VkPipeline* pipeline);

void initShadowPipelineSkinned(VulkanEngine& engine, VkRenderPass& renderpass, VkPipelineLayout pipelineLayout, VkPipeline* pipeline);

void setupShadowDescriptorSetLayouts(VulkanEngine& engine, std::vector<VkDescriptorSetLayout>& setLayoutsOut, VkPipelineLayout* pipelineLayout);

void setupShadowDescriptorSetLayoutsSkinned(VulkanEngine& engine, std::vector<VkDescriptorSetLayout>& setLayoutsOut, VkPipelineLayout* pipelineLayout);

void setupShadowDescriptorSetsSkinned(VulkanEngine& engine, VkBuffer& skinBuffer, VkDescriptorSetLayout setLayout, VkDescriptorSet& descriptorSet);

void setupShadowDescriptorSetsGlobal(VulkanEngine& engine, ShadowFrameResources& shadowFrame, VkBuffer& objectBuffer, std::vector<VkDescriptorSetLayout>& setLayouts);

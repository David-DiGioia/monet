#pragma once

#include "vk_types.h"
#include "vk_engine.h"

// return texture with 6 layers, representing each face of cube
Texture create_cubemap(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent, const std::string& vertPath, const std::string& fragPath);

#pragma once

#include "vk_types.h"
#include "vk_engine.h"

// return texture with 6 layers, representing each face of cube
Texture render_to_texture(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent, bool useMipmap, bool isCubemap, const std::string& vertPath, const std::string& fragPath);

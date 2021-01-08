#pragma once

#include "vk_types.h"
#include "vk_engine.h"

// return texture with 6 layers, representing each face of cube
Texture equirectangular_to_cubemap(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent);

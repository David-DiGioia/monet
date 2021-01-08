#pragma once

#include "vk_types.h"
#include "vk_engine.h"

// return image with 6 layers, representing each face of cube
AllocatedImage equirectangular_to_cubemap(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent);

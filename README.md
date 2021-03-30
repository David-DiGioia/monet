# vulkan-renderer

A C++ game engine made using the Vulkan graphics API.

The purpose of this project is to learn Vulkan and modern rendering techniques.

## Features

 * PBR lighting form HDRI images, rendering all the necessary maps/LUTs at program startup
 * PBR lighting from point lights
 * Instrumented for profiling using Tracy
 * Dear ImGui support for debugging
 * Create materials with arbitrary descriptors by only modifying \_load_materials.txt.
 * PhysX physics implemented
 * Simple audio playback

## Screenshots

Furniture and ground lit by an HDRI
![](showcase/furniture_hdri_00.png?raw=true "fence_synchronization")
![](showcase/furniture_hdri_01.png?raw=true "fence_synchronization")

Camera lit by two different HDRIs
![](showcase/camera_00.png?raw=true "fence_synchronization")

Coffee cart lit by two different HDRIs
![](showcase/coffee_cart_00.png?raw=true "fence_synchronization")

## Plans

 * Shadow mapping
 * SSAO
 * Multisampling
 * Cache assets for faster startup

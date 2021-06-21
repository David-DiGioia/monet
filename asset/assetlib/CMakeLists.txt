set(CMAKE_CXX_STANDARD 17)
# Add source to this project's executable.
add_library (assetlib STATIC 
"texture_asset.h"
"texture_asset.cpp"
"asset_loader.h"
"asset_loader.cpp"
"compression.h"
"compression.cpp"
"vk_mesh.h"
"vk_mesh.cpp"
)

target_include_directories(assetlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(assetlib PUBLIC $<$<CONFIG:Release>:/GL>)
target_link_options(assetlib PUBLIC $<$<CONFIG:Release>:/LTCG>)

target_link_libraries(assetlib PUBLIC json)
target_link_libraries(assetlib PUBLIC vkbootstrap vma glm imgui stb_image spirv_reflect)
target_link_libraries(assetlib PUBLIC Vulkan::Vulkan sdl2 tracy fmt_lib)

target_include_directories(assetlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/lz4/include")
target_include_directories(assetlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
target_link_libraries(assetlib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/lz4/static/liblz4_static.lib")


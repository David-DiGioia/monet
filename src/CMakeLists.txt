
# Add source to this project's executable.
add_executable (vulkan_guide 
 "main.cpp"
 "vk_engine.cpp"
 "vk_engine.h" 
 "vk_types.h" 
 "vk_initializers.cpp" 
 "vk_initializers.h"
 "vk_mesh.cpp"
 "vk_mesh.h"
 "vk_textures.cpp"
 "vk_textures.h"
 "render_to_texture.h"
 "render_to_texture.cpp"
 "../tracy/TracyClient.cpp"
 "application.h"
 "test_app.h"
 "test_app.cpp"
 "physics.h"
 "physics.cpp"
 "util.h"
 "util.cpp"
)


set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC
 "${CMAKE_CURRENT_SOURCE_DIR}"
 "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL2_mixer-2.0.4/include"
 "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/physx/include"
 "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/physx/include_shared"
)

target_link_libraries(vulkan_guide  vkbootstrap vma glm tinyobjloader imgui stb_image)
target_link_libraries(vulkan_guide ${AUDIO_LIB})
target_link_libraries(vulkan_guide
 ${PHYSICS_LIB_00}
 ${PHYSICS_LIB_01}
 ${PHYSICS_LIB_02}
 ${PHYSICS_LIB_03}
 ${PHYSICS_LIB_04}
 ${PHYSICS_LIB_05}
 ${PHYSICS_LIB_06}
 ${PHYSICS_LIB_07}
 ${PHYSICS_LIB_08}
 ${PHYSICS_LIB_09}
 ${PHYSICS_LIB_10}
 ${PHYSICS_LIB_11}
#  ${PHYSICS_LIB_12}
#  ${PHYSICS_LIB_13}
#  ${PHYSICS_LIB_14}
 ${PHYSICS_LIB_15}
 ${PHYSICS_LIB_16}
#  ${PHYSICS_LIB_17}
#  ${PHYSICS_LIB_18}
)

target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2)

add_dependencies(vulkan_guide Shaders)
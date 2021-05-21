set(CMAKE_CXX_STANDARD 17)

# Add source to this project's executable.
file(GLOB ENGINE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable (monet ${ENGINE_FILES})


set_property(TARGET monet PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:monet>")

target_include_directories(monet PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

 
target_compile_definitions(monet PUBLIC TRACY_ENABLE)
target_compile_definitions(monet PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE) 


target_precompile_headers(monet PUBLIC "vk_types.h" "<unordered_map>" "<vector>" "<iostream>" "<fstream>" "<string>" )

target_link_libraries(monet vkbootstrap vma glm tinyobjloader imgui stb_image spirv_reflect)
target_link_libraries(monet Vulkan::Vulkan sdl2 assetlib tracy fmt_lib)

# Custom code below -----------------------------------------------------------------------------

target_include_directories(monet PUBLIC
 "${CMAKE_CURRENT_SOURCE_DIR}"
 "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/physx/include"
 "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/physx/include_shared"

 "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL2_mixer-2.0.4/include"
)

target_link_libraries(monet ${AUDIO_LIB})

target_link_libraries(monet
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
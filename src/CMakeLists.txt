set(CMAKE_CXX_STANDARD 17)

# Add source to this project's executable.
file(GLOB ENGINE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable (monet ${ENGINE_FILES})


set_property(TARGET monet PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:monet>")

target_compile_options(monet PUBLIC $<$<CONFIG:Release>:/GL>)
target_link_options(monet PUBLIC $<$<CONFIG:Release>:/LTCG>)

target_include_directories(monet PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
 
target_compile_definitions(monet PUBLIC TRACY_ENABLE)
target_compile_definitions(monet PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE) 


target_precompile_headers(monet PUBLIC "vk_types.h" "<unordered_map>" "<vector>" "<iostream>" "<fstream>" "<string>" )

target_link_libraries(monet vkbootstrap vma glm imgui stb_image spirv_reflect)
target_link_libraries(monet Vulkan::Vulkan sdl2 assetlib tracy fmt_lib)

# Custom code below -----------------------------------------------------------------------------

target_include_directories(monet PUBLIC
 "${CMAKE_CURRENT_SOURCE_DIR}"
 "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/physx/include"
 "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/physx/include_shared"
)

target_include_directories(monet PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL2_mixer-2.0.4/include")
target_link_libraries(monet "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/SDL2_mixer-2.0.4/lib/x64/SDL2_mixer.lib")

target_include_directories(monet PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cereal")

#target_link_libraries(monet ${AUDIO_LIB})

target_link_libraries(monet
 debug ${PHYSICS_LIB_D_00}
 debug ${PHYSICS_LIB_D_01}
 debug ${PHYSICS_LIB_D_02}
 debug ${PHYSICS_LIB_D_03}
 debug ${PHYSICS_LIB_D_04}
 debug ${PHYSICS_LIB_D_05}
 debug ${PHYSICS_LIB_D_06}
 debug ${PHYSICS_LIB_D_07}
 debug ${PHYSICS_LIB_D_08}
 debug ${PHYSICS_LIB_D_09}
 debug ${PHYSICS_LIB_D_10}
 debug ${PHYSICS_LIB_D_11}
#  debug ${PHYSICS_LIB_D_12}
#  debug ${PHYSICS_LIB_D_13}
#  debug ${PHYSICS_LIB_D_14}
 debug ${PHYSICS_LIB_D_15}
 debug ${PHYSICS_LIB_D_16}
#  debug ${PHYSICS_LIB_D_17}
#  debug ${PHYSICS_LIB_D_18}

optimized ${PHYSICS_LIB_R_00}
optimized ${PHYSICS_LIB_R_01}
optimized ${PHYSICS_LIB_R_02}
optimized ${PHYSICS_LIB_R_03}
optimized ${PHYSICS_LIB_R_04}
optimized ${PHYSICS_LIB_R_05}
optimized ${PHYSICS_LIB_R_06}
optimized ${PHYSICS_LIB_R_07}
optimized ${PHYSICS_LIB_R_08}
optimized ${PHYSICS_LIB_R_09}
optimized ${PHYSICS_LIB_R_10}
optimized ${PHYSICS_LIB_R_11}
#  optimized ${PHYSICS_LIB_R_12}
#  optimized ${PHYSICS_LIB_R_13}
#  optimized ${PHYSICS_LIB_R_14}
optimized ${PHYSICS_LIB_R_15}
optimized ${PHYSICS_LIB_R_16}
#  optimized ${PHYSICS_LIB_R_17}
#  optimized ${PHYSICS_LIB_R_18}
)
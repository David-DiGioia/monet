find_package(Vulkan REQUIRED)
include(ExternalProject)

add_library(vkbootstrap STATIC)
add_library(glm INTERFACE)
add_library(vma INTERFACE)

add_library(stb_image INTERFACE)
add_library(json INTERFACE)
add_library(spirv_reflect STATIC)

add_library(fmt_lib STATIC)

#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/libs")
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/libs")

add_subdirectory(gltf)

set(TRACY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tracy" CACHE FILEPATH "Path to Tracy")
add_library(tracy OBJECT "${TRACY_DIR}/TracyClient.cpp")
target_include_directories(tracy PUBLIC "${TRACY_DIR}")
target_compile_definitions(tracy PUBLIC TRACY_ENABLE)
target_sources(vkbootstrap PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap/VkBootstrap.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap/VkBootstrap.cpp"
)


target_sources(spirv_reflect PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.c"
)

target_include_directories(spirv_reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect" )
target_include_directories(spirv_reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/include" )
target_include_directories(vkbootstrap PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap" )
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan)

#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vma" )

target_include_directories(glm INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/glm" )

target_include_directories(json INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json" )

set(sdl2_DIR "SDL_PATH" CACHE FILEPATH "Path to SDL2")
add_library(sdl2 INTERFACE)
target_include_directories(sdl2 INTERFACE "${sdl2_DIR}/include" "${sdl2_image_DIR}/include" )
target_link_directories(sdl2 INTERFACE "${sdl2_DIR}/lib/x64" "${sdl2_image_DIR}/lib/x64")
target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

target_sources(imgui PRIVATE 
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.h"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_stdlib.h"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_stdlib.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_vulkan.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_sdl.cpp"
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)


target_include_directories(stb_image INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb_image" )


## FMT LIB
target_include_directories(fmt_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/fmt_lib")

file(GLOB FMT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/fmt_lib/fmt/*")

target_sources(fmt_lib PRIVATE ${FMT_FILES})


# Custom code below -----------------------------------------------------------------------------

# *** physx ***
set (BIN_DIR_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/win.x86_64.vc142.md/bin/debug)
set (BIN_DIR_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/win.x86_64.vc142.md/bin/checked)

set (BIN_DIR ${BIN_DIR_DEBUG})
find_library(PHYSICS_LIB_D_00 LowLevel_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_01 LowLevelAABB_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_02 LowLevelDynamics_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_03 PhysX_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_04 PhysXCharacterKinematic_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_05 PhysXCommon_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_06 PhysXCooking_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_07 PhysXExtensions_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_08 PhysXFoundation_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_09 PhysXPvdSDK_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_10 PhysXTask_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_11 PhysXVehicle_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_D_12 SampleBase_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_D_13 SampleFramework_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_D_14 SamplesToolkit_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_15 SceneQuery_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_D_16 SimulationController_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_D_17 SnippetRender_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_D_18 SnippetUtils_static_64 HINTS ${BIN_DIR})

set (BIN_DIR ${BIN_DIR_RELEASE})
find_library(PHYSICS_LIB_R_00 LowLevel_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_01 LowLevelAABB_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_02 LowLevelDynamics_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_03 PhysX_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_04 PhysXCharacterKinematic_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_05 PhysXCommon_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_06 PhysXCooking_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_07 PhysXExtensions_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_08 PhysXFoundation_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_09 PhysXPvdSDK_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_10 PhysXTask_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_11 PhysXVehicle_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_R_12 SampleBase_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_R_13 SampleFramework_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_R_14 SamplesToolkit_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_15 SceneQuery_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_R_16 SimulationController_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_R_17 SnippetRender_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_R_18 SnippetUtils_static_64 HINTS ${BIN_DIR})

find_package(Vulkan REQUIRED)

add_library(vkbootstrap STATIC)
add_library(glm INTERFACE)
add_library(vma INTERFACE)

add_library(stb_image INTERFACE)

add_library(tinyobjloader STATIC)

#add_library(audio SHARED IMPORTED)

#set_target_properties(audio PROPERTIES LINKER_LANGUAGE CXX)

#set_property(TARGET audio PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/SFML-2.5.1/bin/sfml-audio-2.dll")





# For the shared libraries:
# this call will succeed if it finds a SDL2_mixer.lib file
find_library(AUDIO_LIB SDL2_mixer HINTS ${CMAKE_CURRENT_SOURCE_DIR}/SDL2_mixer-2.0.4/lib/x64)
if(NOT AUDIO_LIB)
    message(FATAL_ERROR "Library SDL2_mixer was not found!")
endif()

set (PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL2_mixer-2.0.4/include)

include_directories(${PROJECT_INCLUDE_DIR})

set (BIN_DIR_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/win.x86_64.vc142.md/bin/debug)
set (BIN_DIR_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/win.x86_64.vc142.md/bin/checked)

if (${CONFIG_MODE} MATCHES "debug")
    set (BIN_DIR ${BIN_DIR_DEBUG})
    message(STATUS "PhysX set to Debug mode")
else()
    set (BIN_DIR ${BIN_DIR_RELEASE})
    message(STATUS "PhysX set to Release mode")
endif()

# set(BIN_DIR 
#     $<$<CONFIG:Debug>:BIN_DIR_DEBUG>
#     $<$<CONFIG:RelWithDebInfo>:BIN_DIR_RELEASE>
#     $<$<CONFIG:Release>:BIN_DIR_RELEASE>
#     $<$<CONFIG:MinSizeRel>:BIN_DIR_RELEASE>
# )

find_library(PHYSICS_LIB_00 LowLevel_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_01 LowLevelAABB_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_02 LowLevelDynamics_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_03 PhysX_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_04 PhysXCharacterKinematic_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_05 PhysXCommon_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_06 PhysXCooking_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_07 PhysXExtensions_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_08 PhysXFoundation_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_09 PhysXPvdSDK_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_10 PhysXTask_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_11 PhysXVehicle_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_12 SampleBase_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_13 SampleFramework_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_14 SamplesToolkit_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_15 SceneQuery_static_64 HINTS ${BIN_DIR})
find_library(PHYSICS_LIB_16 SimulationController_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_17 SnippetRender_static_64 HINTS ${BIN_DIR})
# find_library(PHYSICS_LIB_18 SnippetUtils_static_64 HINTS ${BIN_DIR})

# if(NOT PHYSICS_LIB_00)
#     message(FATAL_ERROR "Library PhysX was not found!")
# endif()

# set (PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/physx/include)

# include_directories(${PROJECT_INCLUDE_DIR})


target_sources(vkbootstrap PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap/VkBootstrap.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap/VkBootstrap.cpp"
)

target_include_directories(vkbootstrap PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap" )
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan)

#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vma" )

target_include_directories(glm INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/glm" )

target_sources(tinyobjloader PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader/tiny_obj_loader.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader/tiny_obj_loader.cc"
)

target_include_directories(tinyobjloader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader" )

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

"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_vulkan.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_sdl.cpp"
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)


target_include_directories(stb_image INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb_image" )
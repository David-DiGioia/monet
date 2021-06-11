set(CMAKE_CXX_STANDARD 17)
# Add source to this project's executable.
add_executable (baker
"asset_baker.cpp")

set_property(TARGET baker PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:monet>")

target_compile_options(baker PUBLIC $<$<CONFIG:Release>:/GL>)
target_link_options(baker PUBLIC $<$<CONFIG:Release>:/LTCG>)

target_include_directories(baker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(baker PUBLIC stb_image json assetlib tinyGLTF glm)

target_include_directories(baker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/lz4/include")
target_link_libraries(baker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/lz4/static/liblz4_static.lib")
# lantern_assets — the files a project's scenes use (images, tilesets, sounds,
# fonts): stable asset IDs in .meta files, import, checks, tilesets.
#
# No backend and no UI; always built and unit-tested (also in `core`).

add_library(lantern_assets STATIC
    src/Animation.cpp
    src/AssetDatabase.cpp
    src/Tileset.cpp)
add_library(lantern::assets ALIAS lantern_assets)

target_include_directories(lantern_assets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(lantern_assets SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")
target_link_libraries(lantern_assets
    PUBLIC lantern::scene
    PRIVATE nlohmann_json::nlohmann_json)
lantern_set_warnings(lantern_assets)

# See engine/scene/CMakeLists.txt: nlohmann/json needs char8_t in C++20 mode.
if(MSVC)
    target_compile_options(lantern_assets PRIVATE /Zc:char8_t)
else()
    target_compile_options(lantern_assets PRIVATE -fchar8_t)
endif()

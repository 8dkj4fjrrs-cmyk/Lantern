# lantern_content — RPG content: game variables, dialogues, quests,
# conditions and actions, checking it all, and the player's progress
# (GameState) with save files.
#
# No backend and no UI; always built and unit-tested (also in `core`).

add_library(lantern_content STATIC
    src/Content.cpp
    src/ContentLibrary.cpp
    src/ContentRunner.cpp
    src/GameState.cpp
    src/Logic.cpp)
add_library(lantern::content ALIAS lantern_content)

target_include_directories(lantern_content PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(lantern_content
    PUBLIC lantern::core lantern::scene lantern::assets
    PRIVATE nlohmann_json::nlohmann_json)
lantern_set_warnings(lantern_content)

# See engine/scene/CMakeLists.txt: nlohmann/json needs char8_t in C++20 mode.
if(MSVC)
    target_compile_options(lantern_content PRIVATE /Zc:char8_t)
else()
    target_compile_options(lantern_content PRIVATE -fchar8_t)
endif()

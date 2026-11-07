# rolo_content — RPG content: game variables, dialogues, quests,
# conditions and actions, checking it all, and the player's progress
# (GameState) with save files.
#
# No backend and no UI; always built and unit-tested (also in `core`).

add_library(rolo_content STATIC
    src/Content.cpp
    src/ContentLibrary.cpp
    src/ContentRunner.cpp
    src/GameState.cpp
    src/Logic.cpp
    src/Notation.cpp)
add_library(rolo::content ALIAS rolo_content)

target_include_directories(rolo_content PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(rolo_content
    PUBLIC rolo::core rolo::scene rolo::assets
    PRIVATE nlohmann_json::nlohmann_json)
rolo_set_warnings(rolo_content)

# See engine/scene/CMakeLists.txt: nlohmann/json needs char8_t in C++20 mode.
if(MSVC)
    target_compile_options(rolo_content PRIVATE /Zc:char8_t)
else()
    target_compile_options(rolo_content PRIVATE -fchar8_t)
endif()

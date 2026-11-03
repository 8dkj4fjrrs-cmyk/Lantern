# lantern_gameplay — running a game: behaviours (C++ gameplay code attached
# to objects), input, collision queries, scene changes, and the RPG content
# in scenes (triggers, the player, the dialogue box, save slots).
#
# No backend and no UI; always built and unit-tested (also in `core`).
# Game modules (cmake/LanternGameModules.cmake) build on this library.

add_library(lantern_gameplay STATIC
    src/Collision.cpp
    src/BuiltinBehaviours.cpp
    src/Components.cpp
    src/SaveStore.cpp
    src/Game.cpp
    src/GameModule.cpp
    src/Input.cpp
    src/ScriptApi.cpp
    src/ScriptHost.cpp)
add_library(lantern::gameplay ALIAS lantern_gameplay)

target_include_directories(lantern_gameplay PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(lantern_gameplay
    PUBLIC lantern::core lantern::scene lantern::assets lantern::content
    PRIVATE lantern::lua)   # Lua scripts (Script.hpp); scripts never see Lua's C API
lantern_set_warnings(lantern_gameplay)

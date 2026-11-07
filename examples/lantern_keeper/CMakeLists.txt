# The Lantern Keeper: a small, complete top-down RPG made with Rolo
# (Milestone 7). This module holds the game's C++ behaviours; the game itself
# (scenes, art, dialogue, quests) is in project/. See README.md.

rolo_add_game_module(keeper SOURCES
    src/KeeperBehaviours.cpp
    src/KeeperGame.cpp)
target_include_directories(keeper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

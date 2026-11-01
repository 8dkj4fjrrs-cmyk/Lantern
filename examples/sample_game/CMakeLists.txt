# The sample game module: example behaviours (player movement, doors to
# other scenes, pick-ups, spinning) linked into the editor and the player.
# Its project (scenes and art) is in project/. See README.md.

lantern_add_game_module(sample_game SOURCES
    src/SampleBehaviours.cpp
    src/SampleGame.cpp)
target_include_directories(sample_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

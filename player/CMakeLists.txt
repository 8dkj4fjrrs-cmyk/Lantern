# lantern_player — the game runtime application.
#
#   lantern_player_cli  command-line parsing (no backend; unit-tested)
#   lantern_player      the executable (a .app bundle on macOS)

add_library(lantern_player_cli STATIC runtime/CommandLine.cpp)
target_include_directories(lantern_player_cli PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
target_compile_features(lantern_player_cli PUBLIC cxx_std_20)
lantern_set_warnings(lantern_player_cli)

add_executable(lantern_player runtime/main.cpp)
target_link_libraries(lantern_player PRIVATE lantern_player_cli lantern::runtime)
lantern_set_warnings(lantern_player)

# Axmol's per-app setup: output folder, macOS bundle, and — most importantly —
# copying the compiled shaders next to the program so it can draw anything.
set(APP_NAME lantern_player)
ax_setup_app_config(lantern_player)

if(APPLE)
    set(MACOSX_BUNDLE_BUNDLE_NAME "Lantern Player")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "dev.lantern-engine.player")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 Oscar Hird. MIT License.")
    set_target_properties(lantern_player PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/runtime/Info.plist.in")
endif()

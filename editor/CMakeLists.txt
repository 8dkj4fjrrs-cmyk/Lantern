# The Lantern editor.
#
#   lantern_editor_core  editor logic with no UI toolkit and no backend:
#                        scene documents, undo/redo commands, console log,
#                        preferences, settings paths, command line.
#                        Always built and unit-tested (also in the `core` preset).
#   lantern_editor       the editor application: panels built with Lantern UI
#                        (engine/ui) on top of lantern_runtime. Needs the
#                        Axmol backend.
#
# Dependency rule: the editor may depend on the engine; the engine must
# never depend on the editor.

add_library(lantern_editor_core STATIC
    model/CommandLine.cpp
    model/Commands.cpp
    model/ConsoleLog.cpp
    model/EditorPaths.cpp
    model/EditorState.cpp
    model/GameExport.cpp
    model/LanternResources.cpp
    model/Preferences.cpp
    model/SceneDocument.cpp)
target_include_directories(lantern_editor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/model")
target_link_libraries(lantern_editor_core PUBLIC lantern::core lantern::scene lantern::assets
    PRIVATE nlohmann_json::nlohmann_json)
lantern_set_warnings(lantern_editor_core)
# Only LanternResources.cpp reads JSON (template.json); nlohmann/json needs
# char8_t in C++20 mode (see engine/scene/CMakeLists.txt).
if(MSVC)
    set_source_files_properties(model/LanternResources.cpp PROPERTIES COMPILE_OPTIONS /Zc:char8_t)
else()
    set_source_files_properties(model/LanternResources.cpp PROPERTIES COMPILE_OPTIONS -fchar8_t)
endif()

if(NOT TARGET lantern_runtime)
    message(STATUS "lantern_editor (the window) needs LANTERN_RUNTIME_BACKEND=axmol; building editor logic only.")
    return()
endif()

add_executable(lantern_editor
    application/main.cpp
    application/EditorApp.cpp
    application/Exporting.cpp
    panels/AssetsPanel.cpp
    panels/ConsolePanel.cpp
    panels/FolderPicker.cpp
    panels/HierarchyPanel.cpp
    panels/PanelCommon.cpp
    panels/TilePalettePanel.cpp
    panels/AnimationPanel.cpp
    panels/InspectorPanel.cpp
    panels/InspectorContent.cpp
    panels/ViewportPanel.cpp)

# The runtime gives the editor Lantern's own headers only — never Axmol's —
# so editor code cannot reach into the backend by accident.
target_link_libraries(lantern_editor PRIVATE lantern_editor_core lantern::runtime)
lantern_set_warnings(lantern_editor)
lantern_link_game_modules(lantern_editor)   # behaviours for the Inspector and play mode
# Where a build of the editor (not an installed copy) finds templates,
# examples and licenses (the source tree) and the game runtime it exports
# games with (the player built beside it). See model/LanternResources.hpp.
if(APPLE)
    set(_lantern_dev_player "$<TARGET_BUNDLE_DIR:lantern_player>")
else()
    set(_lantern_dev_player "$<TARGET_FILE:lantern_player>")
endif()
target_compile_definitions(lantern_editor PRIVATE
    LANTERN_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    LANTERN_DEV_PLAYER="${_lantern_dev_player}")
add_dependencies(lantern_editor lantern_player)   # exports need it

# Before Milestone 2 the editor was a single file at bin/lantern_editor; it is
# now a folder (program + shaders). Remove the old file from existing builds.
if(EXISTS "${CMAKE_BINARY_DIR}/bin/lantern_editor" AND NOT IS_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lantern_editor")
    file(REMOVE "${CMAKE_BINARY_DIR}/bin/lantern_editor")
endif()

# Axmol's per-app setup (output folder, macOS bundle, compiled shaders).
set(APP_NAME lantern_editor)
set(_AX_EXTENSION_LIBS "")   # see player/CMakeLists.txt
ax_setup_app_config(lantern_editor)

if(APPLE)
    set(MACOSX_BUNDLE_BUNDLE_NAME "Lantern Editor")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "dev.lantern-engine.editor")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 Oscar Hird. MIT License.")
    set_target_properties(lantern_editor PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in")
endif()

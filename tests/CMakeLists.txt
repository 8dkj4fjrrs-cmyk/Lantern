# Unit tests (Catch2 v3) and smoke tests.
#
# Catch2 is used if already installed (e.g. `brew install catch2`); otherwise
# CMake downloads the pinned release below. It is a test-only dependency and
# is never linked into the engine, editor, or shipped games.

include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE
    FIND_PACKAGE_ARGS 3)
FetchContent_MakeAvailable(Catch2)
include(Catch)

# ---------------------------------------------------------------------------
# Core and editor-logic tests: no backend, no window. Always built.
# ---------------------------------------------------------------------------
add_executable(lantern_tests
    core/test_engine.cpp
    core/test_log.cpp
    core/test_version.cpp
    editor/test_console_log.cpp
    editor/test_editor_paths_and_cli.cpp
    editor/test_preferences.cpp
    ui/test_ui_context.cpp
    ui/test_ui_docking.cpp
    ui/test_ui_foundations.cpp
    ui/test_ui_widgets.cpp
    ui/test_ui_inspector.cpp
    editor/test_commands.cpp
    scene/test_scene.cpp
    scene/test_scene_file.cpp
    scene/test_project.cpp
    scene/test_tilemap.cpp
    assets/test_assets.cpp
    gameplay/test_input.cpp
    gameplay/test_collision.cpp
    gameplay/test_game.cpp
    gameplay/test_game_content.cpp
    content/test_logic.cpp
    content/test_content_files.cpp
    content/test_content_library.cpp
    content/test_content_runner.cpp
    content/test_save.cpp)
target_link_libraries(lantern_tests PRIVATE lantern::core lantern::scene lantern::assets lantern::content lantern::gameplay lantern::ui lantern_editor_core Catch2::Catch2WithMain)
if(TARGET sample_game)
    target_sources(lantern_tests PRIVATE gameplay/test_sample_game.cpp)
    lantern_link_game_modules(lantern_tests MODULES sample_game)
    target_compile_definitions(lantern_tests PRIVATE LANTERN_SAMPLE_PROJECT_DIR="${PROJECT_SOURCE_DIR}/examples/sample_game/project")
else()
    lantern_link_game_modules(lantern_tests MODULES)
endif()
target_compile_definitions(lantern_tests PRIVATE LANTERN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
lantern_set_warnings(lantern_tests)
catch_discover_tests(lantern_tests)

# ---------------------------------------------------------------------------
# Runtime, player and editor tests: only when the Axmol backend is built.
# ---------------------------------------------------------------------------
if(NOT TARGET lantern_player)
    return()
endif()

# Unit tests that need no window.
add_executable(lantern_runtime_tests
    player/test_command_line.cpp
    runtime/test_log_bridge.cpp
    runtime/test_scene_renderer.cpp)
target_link_libraries(lantern_runtime_tests PRIVATE
    lantern_player_cli lantern::runtime ${_AX_CORE_LIB} Catch2::Catch2WithMain)
# These tests check internal headers of lantern_runtime.
target_include_directories(lantern_runtime_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/runtime/src")
lantern_set_warnings(lantern_runtime_tests)
catch_discover_tests(lantern_runtime_tests)

add_test(NAME player.version COMMAND lantern_player --version)
set_tests_properties(player.version PROPERTIES PASS_REGULAR_EXPRESSION "backend: Axmol")

add_test(NAME player.rejects_bad_option COMMAND lantern_player --frames zero)
set_tests_properties(player.rejects_bad_option PROPERTIES WILL_FAIL TRUE)

add_test(NAME player.plays_scene_file
    COMMAND lantern_player --scene "${CMAKE_CURRENT_SOURCE_DIR}/data/test_card.lscene" --frames 30
            --screenshot "${CMAKE_CURRENT_BINARY_DIR}/player_test_card.png")
set_tests_properties(player.plays_scene_file PROPERTIES
    PASS_REGULAR_EXPRESSION "Showing scene 'Test Card' \\(4 entities\\).*Screenshot saved"
    FAIL_REGULAR_EXPRESSION "\\[ERROR\\]|\\[WARN"
    TIMEOUT 60)

add_test(NAME player.rejects_missing_scene COMMAND lantern_player --scene "no-such-file.lscene")
set_tests_properties(player.rejects_missing_scene PROPERTIES WILL_FAIL TRUE)

if(TARGET lantern_editor)
    add_test(NAME editor.version COMMAND lantern_editor --version)
    set_tests_properties(editor.version PROPERTIES
        PASS_REGULAR_EXPRESSION "Lantern Editor [0-9]+\\.[0-9]+\\.[0-9]+ \\(backend: Axmol")

    add_test(NAME editor.rejects_unknown_option COMMAND lantern_editor --no-such-flag)
    set_tests_properties(editor.rejects_unknown_option PROPERTIES WILL_FAIL TRUE)
endif()

# Window tests: open a real window for a few frames. They need a screen
# (on headless Linux CI, run ctest under `xvfb-run`).
if(NOT LANTERN_WINDOW_TESTS)
    return()
endif()

add_test(NAME player.window_smoke COMMAND lantern_player --frames 120)
set_tests_properties(player.window_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "Window closed after 12[0-9] frames"
    FAIL_REGULAR_EXPRESSION "\\[ERROR\\]"
    TIMEOUT 60)

add_test(NAME player.screenshot
    COMMAND lantern_player --frames 90 --screenshot "${CMAKE_CURRENT_BINARY_DIR}/player_screenshot.png")
set_tests_properties(player.screenshot PROPERTIES
    PASS_REGULAR_EXPRESSION "Screenshot saved"
    FAIL_REGULAR_EXPRESSION "\\[ERROR\\]"
    TIMEOUT 60)

if(TARGET sample_game)
    # Gameplay end to end (fixed 1/60 s frames so every run is the same):
    # the player walks up and right to the elder, talks (E), goes on, and
    # accepts the quest. The screenshot shows the dialogue box.
    add_test(NAME player.plays_sample_game
        COMMAND lantern_player --project "${PROJECT_SOURCE_DIR}/examples/sample_game/project" --frames 200
                --fixed-step --no-stats --save-dir "${CMAKE_CURRENT_BINARY_DIR}/player-test-saves"
                --tap Up@5-34 --tap Right@40-100 --tap E@120 --tap E@140 --tap E@170
                --screenshot "${CMAKE_CURRENT_BINARY_DIR}/player_sample_dialogue.png")
    set_tests_properties(player.plays_sample_game PROPERTIES
        PASS_REGULAR_EXPRESSION "Showing scene 'Meadow'.*Quest started: Gems for the Elder.*Screenshot saved"
        FAIL_REGULAR_EXPRESSION "\\[ERROR\\]|\\[WARN"
        TIMEOUT 120)
endif()

if(TARGET lantern_editor)
    # Editor tests keep their settings in the build folder, never in your real
    # user settings.
    set(_editor_cfg "${CMAKE_CURRENT_BINARY_DIR}/editor-test-settings")

    # 1) First run: default layout, then layout + preferences are saved.
    add_test(NAME editor.first_run
        COMMAND lantern_editor --config-dir "${_editor_cfg}" --no-project --reset-layout --frames 120
                --screenshot "${CMAKE_CURRENT_BINARY_DIR}/editor_screenshot.png")
    set_tests_properties(editor.first_run PROPERTIES
        PASS_REGULAR_EXPRESSION "Using the default panel layout.*Screenshot saved"
        FAIL_REGULAR_EXPRESSION "\\[ERROR\\]"
        FIXTURES_SETUP editor_settings
        TIMEOUT 60)

    # 2) Second run: the saved layout must be restored (persistence works).
    add_test(NAME editor.restores_layout
        COMMAND lantern_editor --config-dir "${_editor_cfg}" --no-project --frames 60)
    set_tests_properties(editor.restores_layout PROPERTIES
        PASS_REGULAR_EXPRESSION "Restored the saved panel layout"
        FAIL_REGULAR_EXPRESSION "\\[ERROR\\]|Using the default panel layout"
        FIXTURES_REQUIRED editor_settings
        TIMEOUT 60)

    # 3) Projects, end to end: the editor creates one, opens it again (as the
    #    last project used), and the player plays its start scene.
    set(_project "${CMAKE_CURRENT_BINARY_DIR}/test-projects/Window Test Game")
    set(_project_cfg "${CMAKE_CURRENT_BINARY_DIR}/editor-test-project-settings")
    add_test(NAME editor.project_cleanup
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/test-projects" "${_project_cfg}")
    set_tests_properties(editor.project_cleanup PROPERTIES FIXTURES_SETUP editor_project_clean)

    add_test(NAME editor.creates_project
        COMMAND lantern_editor --config-dir "${_project_cfg}" --new-project "${_project}" --frames 30)
    set_tests_properties(editor.creates_project PROPERTIES
        PASS_REGULAR_EXPRESSION "Created project in.*Opened scene main.lscene.*Showing scene 'Main'"
        FAIL_REGULAR_EXPRESSION "\\[ERROR\\]|\\[WARN"
        FIXTURES_REQUIRED editor_project_clean
        FIXTURES_SETUP editor_project
        TIMEOUT 60)

    add_test(NAME editor.reopens_last_project
        COMMAND lantern_editor --config-dir "${_project_cfg}" --frames 30)
    set_tests_properties(editor.reopens_last_project PROPERTIES
        PASS_REGULAR_EXPRESSION "Opened project 'Window Test Game'.*Opened scene main.lscene"
        FAIL_REGULAR_EXPRESSION "\\[ERROR\\]|\\[WARN"
        FIXTURES_REQUIRED editor_project
        TIMEOUT 60)

    if(TARGET sample_game)
        # Play mode: the editor opens the sample, plays it, and the game's
        # behaviours run without errors; quitting stops the game.
        add_test(NAME editor.plays_sample_game
            COMMAND lantern_editor --config-dir "${CMAKE_CURRENT_BINARY_DIR}/editor-test-play-settings"
                    --project "${PROJECT_SOURCE_DIR}/examples/sample_game/project" --play --frames 60
                    --screenshot "${CMAKE_CURRENT_BINARY_DIR}/editor_playing.png")
        set_tests_properties(editor.plays_sample_game PROPERTIES
            PASS_REGULAR_EXPRESSION "Game module: sample_game.*Playing 'Meadow'.*Screenshot saved.*Stopped playing after [0-9]+ frames"
            FAIL_REGULAR_EXPRESSION "\\[ERROR\\]|\\[WARN"
            TIMEOUT 60)
    endif()

    add_test(NAME player.plays_project
        COMMAND lantern_player --project "${_project}" --frames 30)
    set_tests_properties(player.plays_project PROPERTIES
        PASS_REGULAR_EXPRESSION "Showing scene 'Main' \\(18 entities\\)"
        FAIL_REGULAR_EXPRESSION "\\[ERROR\\]|\\[WARN"
        FIXTURES_REQUIRED editor_project
        TIMEOUT 60)
endif()

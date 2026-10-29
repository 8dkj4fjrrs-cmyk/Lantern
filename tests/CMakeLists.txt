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
    ui/test_ui_widgets.cpp)
target_link_libraries(lantern_tests PRIVATE lantern::core lantern::ui lantern_editor_core Catch2::Catch2WithMain)
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
    runtime/test_scene_outline.cpp)
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

if(TARGET lantern_editor)
    # Editor tests keep their settings in the build folder, never in your real
    # user settings.
    set(_editor_cfg "${CMAKE_CURRENT_BINARY_DIR}/editor-test-settings")

    # 1) First run: default layout, then layout + preferences are saved.
    add_test(NAME editor.first_run
        COMMAND lantern_editor --config-dir "${_editor_cfg}" --reset-layout --frames 120
                --screenshot "${CMAKE_CURRENT_BINARY_DIR}/editor_screenshot.png")
    set_tests_properties(editor.first_run PROPERTIES
        PASS_REGULAR_EXPRESSION "Using the default panel layout.*Screenshot saved"
        FAIL_REGULAR_EXPRESSION "\\[ERROR\\]"
        FIXTURES_SETUP editor_settings
        TIMEOUT 60)

    # 2) Second run: the saved layout must be restored (persistence works).
    add_test(NAME editor.restores_layout
        COMMAND lantern_editor --config-dir "${_editor_cfg}" --frames 60)
    set_tests_properties(editor.restores_layout PROPERTIES
        PASS_REGULAR_EXPRESSION "Restored the saved panel layout"
        FAIL_REGULAR_EXPRESSION "\\[ERROR\\]|Using the default panel layout"
        FIXTURES_REQUIRED editor_settings
        TIMEOUT 60)
endif()

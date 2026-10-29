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
# Core tests: no backend, no window. Always built.
# ---------------------------------------------------------------------------
add_executable(lantern_tests
    core/test_engine.cpp
    core/test_log.cpp
    core/test_version.cpp)
target_link_libraries(lantern_tests PRIVATE lantern::core Catch2::Catch2WithMain)
lantern_set_warnings(lantern_tests)
catch_discover_tests(lantern_tests)

# Editor smoke tests: the editor binary must start and stop cleanly.
if(TARGET lantern_editor)
    add_test(NAME editor.version COMMAND lantern_editor --version)
    set_tests_properties(editor.version PROPERTIES PASS_REGULAR_EXPRESSION "Lantern Editor [0-9]+\\.[0-9]+\\.[0-9]+")

    add_test(NAME editor.smoke COMMAND lantern_editor --smoke-test)
    set_tests_properties(editor.smoke PROPERTIES PASS_REGULAR_EXPRESSION "Smoke test passed")

    add_test(NAME editor.rejects_unknown_option COMMAND lantern_editor --no-such-flag)
    set_tests_properties(editor.rejects_unknown_option PROPERTIES WILL_FAIL TRUE)
endif()

# ---------------------------------------------------------------------------
# Runtime tests: only when the Axmol backend is built.
# ---------------------------------------------------------------------------
if(TARGET lantern_player)
    # Unit tests that need no window.
    add_executable(lantern_runtime_tests
        player/test_command_line.cpp
        runtime/test_log_bridge.cpp)
    target_link_libraries(lantern_runtime_tests PRIVATE
        lantern_player_cli lantern::runtime ${_AX_CORE_LIB} Catch2::Catch2WithMain)
    # The log-bridge test checks an internal header of lantern_runtime.
    target_include_directories(lantern_runtime_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/runtime/src")
    lantern_set_warnings(lantern_runtime_tests)
    catch_discover_tests(lantern_runtime_tests)

    add_test(NAME player.version COMMAND lantern_player --version)
    set_tests_properties(player.version PROPERTIES PASS_REGULAR_EXPRESSION "backend: Axmol")

    add_test(NAME player.rejects_bad_option COMMAND lantern_player --frames zero)
    set_tests_properties(player.rejects_bad_option PROPERTIES WILL_FAIL TRUE)

    # Window tests: open a real window for a few frames. They need a screen
    # (on headless Linux CI, run ctest under `xvfb-run`).
    if(LANTERN_WINDOW_TESTS)
        add_test(NAME player.window_smoke COMMAND lantern_player --frames 120)
        set_tests_properties(player.window_smoke PROPERTIES
            PASS_REGULAR_EXPRESSION "Window closed after 12[0-9] frames"
            FAIL_REGULAR_EXPRESSION "\\[ERROR\\]"
            TIMEOUT 60)

        set(_shot "${CMAKE_CURRENT_BINARY_DIR}/player_screenshot.png")
        add_test(NAME player.screenshot
            COMMAND lantern_player --frames 90 --screenshot "${_shot}")
        set_tests_properties(player.screenshot PROPERTIES
            PASS_REGULAR_EXPRESSION "Screenshot saved"
            FAIL_REGULAR_EXPRESSION "\\[ERROR\\]"
            TIMEOUT 60)
    endif()
endif()

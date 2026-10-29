# Unit tests (Catch2 v3).
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

add_executable(lantern_tests
    core/test_engine.cpp
    core/test_log.cpp
    core/test_version.cpp)

target_link_libraries(lantern_tests PRIVATE lantern::core Catch2::Catch2WithMain)
lantern_set_warnings(lantern_tests)

include(Catch)
catch_discover_tests(lantern_tests)

# Smoke tests: the editor binary must start and stop cleanly.
if(TARGET lantern_editor)
    add_test(NAME editor.version COMMAND lantern_editor --version)
    set_tests_properties(editor.version PROPERTIES PASS_REGULAR_EXPRESSION "Lantern Editor [0-9]+\\.[0-9]+\\.[0-9]+")

    add_test(NAME editor.smoke COMMAND lantern_editor --smoke-test)
    set_tests_properties(editor.smoke PROPERTIES PASS_REGULAR_EXPRESSION "Smoke test passed")

    add_test(NAME editor.rejects_unknown_option COMMAND lantern_editor --no-such-flag)
    set_tests_properties(editor.rejects_unknown_option PROPERTIES WILL_FAIL TRUE)
endif()

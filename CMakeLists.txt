# Lantern 2D Engine — root build configuration.
#
# Targets (Milestone 0):
#   lantern_core    static library: engine lifecycle, logging, version info
#   lantern_editor  standalone editor executable (console-only until Milestone 2)
#   lantern_tests   Catch2 unit tests (registered with CTest)
#
# The Cocos-family runtime backend is NOT wired in yet. See
# docs/adr/0001-runtime-foundation.md for why, and LANTERN_RUNTIME_BACKEND below.

cmake_minimum_required(VERSION 3.24)

project(Lantern
    VERSION 0.0.1
    DESCRIPTION "A C++-first 2D game engine and editor"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets editors/clangd understand the code

# Put all executables in one folder so they are easy to find after a build.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LanternCompilerWarnings)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(LANTERN_BUILD_EDITOR "Build the standalone editor" ON)
option(LANTERN_BUILD_TESTS "Build unit tests" ON)
option(LANTERN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LANTERN_ENABLE_SANITIZERS "Build with AddressSanitizer + UBSan (Clang/GCC)" OFF)

# Which rendering/runtime backend to link. Only "none" exists today; the
# choice between upstream Cocos2d-x and a maintained fork is pending
# (docs/adr/0001-runtime-foundation.md).
set(LANTERN_RUNTIME_BACKEND "none" CACHE STRING "Runtime backend to link")
set_property(CACHE LANTERN_RUNTIME_BACKEND PROPERTY STRINGS none)
if(NOT LANTERN_RUNTIME_BACKEND STREQUAL "none")
    message(FATAL_ERROR
        "LANTERN_RUNTIME_BACKEND='${LANTERN_RUNTIME_BACKEND}' is not implemented yet. "
        "Only 'none' is available in Milestone 0. See docs/adr/0001-runtime-foundation.md.")
endif()

if(LANTERN_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Modules
# ---------------------------------------------------------------------------
add_subdirectory(engine/core)

if(LANTERN_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(LANTERN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Lantern ${PROJECT_VERSION}: editor=${LANTERN_BUILD_EDITOR} "
               "tests=${LANTERN_BUILD_TESTS} backend=${LANTERN_RUNTIME_BACKEND}")

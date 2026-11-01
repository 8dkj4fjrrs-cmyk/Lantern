# Lantern Engine — root build configuration.
#
# Targets:
#   lantern_core     static library: engine lifecycle, logging, version info.
#                    Depends only on the C++ standard library.
#   lantern_ui       static library: Lantern's own UI toolkit (widgets, docking,
#                    text). No backend; always built and unit-tested.
#   lantern_gameplay static library: behaviours, input, collision queries,
#                    scene changes (no backend; always built and tested).
#   sample_game      the sample game module (examples/sample_game).
#   lantern_runtime  static library: window, main loop, rendering backend.
#                    Wraps Axmol privately (only with LANTERN_RUNTIME_BACKEND=axmol).
#   lantern_player   the game runtime application (opens a window).
#   lantern_editor_core  editor logic without UI code (always built, unit-tested).
#   lantern_editor   the standalone editor application (built with lantern_ui).
#   lantern_tests    Catch2 unit tests (registered with CTest).
#
# Backend background: docs/adr/0001-runtime-foundation.md

cmake_minimum_required(VERSION 3.25)

project(Lantern
    VERSION 0.5.0
    DESCRIPTION "Lantern Engine: a C++-first 2D game engine and editor"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets editors/clangd understand the code

# Put our executables in one folder so they are easy to find after a build.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LanternCompilerWarnings)
include(LanternGameModules)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(LANTERN_BUILD_EDITOR "Build the standalone editor" ON)
option(LANTERN_BUILD_TESTS "Build unit tests" ON)
option(LANTERN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LANTERN_ENABLE_SANITIZERS "Build with AddressSanitizer + UBSan (Clang/GCC)" OFF)
option(LANTERN_WINDOW_TESTS "Register tests that open a real window (needs a display)" ON)
option(LANTERN_BUILD_SAMPLE_GAME "Build the sample game module and link it into the editor and player" ON)
# Folders with your own game modules (each has a CMakeLists.txt calling
# lantern_add_game_module); they are linked into the editor and the player.
set(LANTERN_GAME_MODULE_DIRS "" CACHE STRING "Folders containing game modules to build in (separated by ;)")

# Which rendering/runtime backend to link:
#   axmol  the real engine runtime (default)
#   none   core library, editor shell and tests only; no window, very fast to build
set(LANTERN_RUNTIME_BACKEND "axmol" CACHE STRING "Runtime backend to link")
set_property(CACHE LANTERN_RUNTIME_BACKEND PROPERTY STRINGS axmol none)
if(NOT LANTERN_RUNTIME_BACKEND MATCHES "^(axmol|none)$")
    message(FATAL_ERROR "LANTERN_RUNTIME_BACKEND must be 'axmol' or 'none', not '${LANTERN_RUNTIME_BACKEND}'.")
endif()

if(LANTERN_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Third-party backend
# ---------------------------------------------------------------------------
if(LANTERN_RUNTIME_BACKEND STREQUAL "axmol")
    if(APPLE)
        enable_language(OBJC OBJCXX)   # Axmol's macOS platform layer is Objective-C++
    endif()
    include(LanternAxmol)
    set(LANTERN_BACKEND_DISPLAY_NAME "Axmol ${LANTERN_AXMOL_VERSION}")
else()
    set(LANTERN_BACKEND_DISPLAY_NAME "none")
endif()

# ---------------------------------------------------------------------------
# Our modules
# ---------------------------------------------------------------------------
add_subdirectory(engine/core)
add_subdirectory(engine/scene)
add_subdirectory(engine/assets)
add_subdirectory(engine/gameplay)
add_subdirectory(engine/ui)

# Game modules come before the programs that link them.
if(LANTERN_BUILD_SAMPLE_GAME)
    add_subdirectory(examples/sample_game)
endif()
foreach(dir IN LISTS LANTERN_GAME_MODULE_DIRS)
    get_filename_component(_name "${dir}" NAME)
    add_subdirectory("${dir}" "${CMAKE_BINARY_DIR}/game_modules/${_name}")
endforeach()

if(LANTERN_RUNTIME_BACKEND STREQUAL "axmol")
    add_subdirectory(engine/runtime)
    add_subdirectory(player)
endif()

if(LANTERN_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(LANTERN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Lantern ${PROJECT_VERSION}: backend=${LANTERN_BACKEND_DISPLAY_NAME} "
               "editor=${LANTERN_BUILD_EDITOR} tests=${LANTERN_BUILD_TESTS}")

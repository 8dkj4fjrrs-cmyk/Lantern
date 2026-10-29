# lantern_core — the lowest engine layer.
#
# Rules for this module:
#   * No editor code, no UI code, no Cocos types in public headers.
#   * Everything else in the engine may depend on it; it depends on nothing
#     except the C++ standard library.

configure_file(
    include/lantern/core/Version.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/lantern/core/Version.hpp"
    @ONLY)

add_library(lantern_core STATIC
    src/Engine.cpp
    src/Files.cpp
    src/Numbers.cpp
    src/Log.cpp
    src/Version.cpp)

add_library(lantern::core ALIAS lantern_core)

target_include_directories(lantern_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_compile_features(lantern_core PUBLIC cxx_std_20)
lantern_set_warnings(lantern_core)

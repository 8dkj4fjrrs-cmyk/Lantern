# lantern_runtime — window, main loop and rendering, built on Axmol.
#
# Axmol is linked PRIVATELY: code that uses lantern_runtime sees only
# include/lantern/runtime/*.hpp, never Axmol headers.

add_library(lantern_runtime STATIC
    src/Application.cpp
    src/AxmolLogBridge.cpp
    src/StartScene.cpp)

add_library(lantern::runtime ALIAS lantern_runtime)

target_include_directories(lantern_runtime
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(lantern_runtime
    PUBLIC lantern::core
    PRIVATE ${_AX_CORE_LIB})

lantern_set_warnings(lantern_runtime)

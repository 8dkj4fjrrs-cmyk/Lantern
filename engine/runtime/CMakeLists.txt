# lantern_runtime — window, main loop, rendering and the optional Lantern UI
# layer, built on Axmol.
#
# Axmol is linked PRIVATELY: code that uses lantern_runtime sees only
# include/lantern/runtime/*.hpp and Lantern's own libraries, never Axmol.

add_library(lantern_runtime STATIC
    src/Application.cpp
    src/AxmolLogBridge.cpp
    src/SceneRenderer.cpp
    src/UiHost.cpp)

add_library(lantern::runtime ALIAS lantern_runtime)

target_include_directories(lantern_runtime
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(lantern_runtime
    PUBLIC lantern::core lantern::scene lantern::ui
    PRIVATE ${_AX_CORE_LIB})

lantern_set_warnings(lantern_runtime)

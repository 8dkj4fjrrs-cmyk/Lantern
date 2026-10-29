# lantern_runtime — window, main loop, rendering and the optional
# immediate-mode UI layer, built on Axmol and its Dear ImGui extension.
#
# Axmol and ImGui are linked PRIVATELY: code that uses lantern_runtime sees
# only include/lantern/runtime/*.hpp, never Axmol or ImGui headers.

add_library(lantern_runtime STATIC
    src/Application.cpp
    src/AxmolLogBridge.cpp
    src/SceneOutline.cpp
    src/StartScene.cpp)

add_library(lantern::runtime ALIAS lantern_runtime)

target_include_directories(lantern_runtime
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(lantern_runtime
    PUBLIC lantern::core
    PRIVATE ${_AX_CORE_LIB} ImGui)

lantern_set_warnings(lantern_runtime)

# lantern_editor — the standalone editor application.
#
# Milestone 0: a console entry point that proves the editor target links
# against the engine and starts/stops it. The Dear ImGui window, docking
# and panels arrive in Milestone 2.
#
# Dependency rule: the editor may depend on the engine; the engine must
# never depend on the editor.

add_executable(lantern_editor
    application/main.cpp)

target_link_libraries(lantern_editor PRIVATE lantern::core)
lantern_set_warnings(lantern_editor)

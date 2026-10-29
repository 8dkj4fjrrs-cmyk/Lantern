# lantern_ui — Lantern's own user-interface toolkit.
#
# Pure C++ with no window, GPU or backend code: widgets produce a DrawList
# (triangles + textures) and consume Lantern input events. lantern_runtime
# connects it to the real window. Because of that, everything here is
# unit-tested without a display, in every build preset.
#
# Third-party pieces (kept separate, see THIRD_PARTY_NOTICES.md):
#   third_party/stb/stb_truetype.h  font rasteriser (public domain / MIT)
#   assets/fonts/Inter-*.ttf         UI font (SIL Open Font License 1.1)

include(LanternEmbed)

add_library(lantern_ui STATIC
    src/Context.cpp
    src/DockLayout.cpp
    src/DrawList.cpp
    src/EmbeddedFonts.cpp
    src/Font.cpp
    src/Icons.cpp
    src/Input.cpp
    src/StbTrueTypeImpl.cpp
    src/Utf8.cpp
    src/Widget.cpp
    src/widgets/Basic.cpp
    src/widgets/Box.cpp
    src/widgets/Dialog.cpp
    src/widgets/DockSpace.cpp
    src/widgets/Menu.cpp
    src/widgets/Scroll.cpp
    src/widgets/TextField.cpp
    src/widgets/Tree.cpp)

add_library(lantern::ui ALIAS lantern_ui)

lantern_embed_files(lantern_ui
    NAMESPACE lantern::ui::embedded
    FILES
        inter_regular=${PROJECT_SOURCE_DIR}/assets/fonts/Inter-Regular.ttf
        inter_semibold=${PROJECT_SOURCE_DIR}/assets/fonts/Inter-SemiBold.ttf)

target_include_directories(lantern_ui
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(lantern_ui SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")
target_compile_features(lantern_ui PUBLIC cxx_std_20)
lantern_set_warnings(lantern_ui)

# Third-party and generated files are not held to our warning rules.
set_source_files_properties(src/StbTrueTypeImpl.cpp PROPERTIES COMPILE_OPTIONS "-w")
if(MSVC)
    set_source_files_properties(src/StbTrueTypeImpl.cpp PROPERTIES COMPILE_OPTIONS "/w")
endif()

# Installing and packaging Lantern: what someone downloads.
#
#   cmake --build --preset release --target lantern_package
#
# makes dist/Lantern-<version>-<system>/ and a .zip of it. The layout is what
# the editor expects to find beside itself (editor/model/LanternResources.hpp):
#
#   Linux, Windows                         macOS
#   Lantern-<v>-<system>/                  Lantern-<v>-macOS/
#     lantern_editor(.exe), axslc/           lantern_editor.app/  (Contents/Resources/lantern/...)
#     runtime/lantern_player(.exe), axslc/     ... runtime/lantern_player.app
#     templates/ examples/ licenses/ docs/   README.md, LICENSE, ...
#     README.md LICENSE THIRD_PARTY_NOTICES.md CHANGELOG.md
#
# Every rule belongs to the "lantern" component: libraries bundled by Axmol
# have install rules of their own (headers, static libraries), and none of
# those belong in the download.

if(NOT TARGET lantern_editor OR NOT TARGET lantern_player)
    return()
endif()

set(_component COMPONENT lantern)
if(APPLE)
    set(_resources "lantern_editor.app/Contents/Resources/lantern")
    install(TARGETS lantern_editor BUNDLE DESTINATION . ${_component})
    install(TARGETS lantern_player BUNDLE DESTINATION "${_resources}/runtime" ${_component})
else()
    set(_resources ".")
    install(TARGETS lantern_editor RUNTIME DESTINATION . ${_component})
    install(TARGETS lantern_player RUNTIME DESTINATION runtime ${_component})
    # The compiled shaders both programs load from beside themselves.
    install(DIRECTORY "${AXSLCC_OUT_DIR}/" DESTINATION axslc ${_component})
    install(DIRECTORY "${AXSLCC_OUT_DIR}/" DESTINATION runtime/axslc ${_component})
    if(WIN32)
        # DLLs Axmol puts beside its programs (e.g. ANGLE's OpenGL ES on Windows).
        install(DIRECTORY "$<TARGET_FILE_DIR:lantern_editor>/" DESTINATION . ${_component} FILES_MATCHING PATTERN "*.dll")
        install(DIRECTORY "$<TARGET_FILE_DIR:lantern_player>/" DESTINATION runtime ${_component}
                FILES_MATCHING PATTERN "*.dll")
    endif()
endif()

# Templates, examples (projects and their C++ modules, to learn from),
# licenses and documentation.
install(DIRECTORY templates/ DESTINATION "${_resources}/templates" ${_component}
        PATTERN "__pycache__" EXCLUDE)
install(DIRECTORY examples/ DESTINATION "${_resources}/examples" ${_component}
        PATTERN "__pycache__" EXCLUDE)
install(DIRECTORY licenses/ DESTINATION "${_resources}/licenses" ${_component})
install(DIRECTORY docs/ DESTINATION "${_resources}/docs" ${_component})
install(FILES README.md LICENSE THIRD_PARTY_NOTICES.md CHANGELOG.md DESTINATION . ${_component})

# The package: install into dist/<name>, then zip it.
if(APPLE)
    set(_system "macOS")
elseif(WIN32)
    set(_system "Windows")
else()
    set(_system "Linux")
endif()
set(LANTERN_PACKAGE_NAME "Lantern-${PROJECT_VERSION}${LANTERN_VERSION_SUFFIX}-${_system}-${CMAKE_SYSTEM_PROCESSOR}")
set(_dist "${PROJECT_SOURCE_DIR}/dist")
add_custom_target(lantern_package
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_dist}/${LANTERN_PACKAGE_NAME}" "${_dist}/${LANTERN_PACKAGE_NAME}.zip"
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --component lantern --config $<CONFIG>
            --prefix "${_dist}/${LANTERN_PACKAGE_NAME}"
    COMMAND "${CMAKE_COMMAND}" -E chdir "${_dist}" "${CMAKE_COMMAND}" -E tar cf "${LANTERN_PACKAGE_NAME}.zip"
            --format=zip "${LANTERN_PACKAGE_NAME}"
    COMMENT "Packaging ${LANTERN_PACKAGE_NAME} into ${_dist}"
    VERBATIM)
add_dependencies(lantern_package lantern_editor lantern_player)

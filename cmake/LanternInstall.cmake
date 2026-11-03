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
        # The DLLs the programs link (Axmol's prebuilt zlib1.dll, for one).
        # A Windows program only finds DLLs beside it, so both folders get them;
        # exported games copy runtime/ (tools/release/check_windows_dlls.py
        # checks the package).
        install(FILES $<TARGET_RUNTIME_DLLS:lantern_editor> DESTINATION . ${_component})
        install(FILES $<TARGET_RUNTIME_DLLS:lantern_player> DESTINATION runtime ${_component})
        # The Visual C++ runtime (msvcp140.dll, vcruntime140*.dll): not every
        # PC has it installed, and Microsoft allows shipping it beside a program.
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
        set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)   # the Universal CRT is part of Windows 10 and later
        include(InstallRequiredSystemLibraries)
        if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
            install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION . ${_component})
            install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION runtime ${_component})
        endif()
        # DLLs Axmol puts beside its programs (e.g. ANGLE's OpenGL ES on Windows).
        # The build folder also has an "axslc" link to the shaders there;
        # it is left out, since the real shaders are installed above (CMake
        # refuses to install a link over that folder).
        install(DIRECTORY "$<TARGET_FILE_DIR:lantern_editor>/" DESTINATION . ${_component}
                FILES_MATCHING PATTERN "*.dll" PATTERN "axslc" EXCLUDE)
        install(DIRECTORY "$<TARGET_FILE_DIR:lantern_player>/" DESTINATION runtime ${_component}
                FILES_MATCHING PATTERN "*.dll" PATTERN "axslc" EXCLUDE)
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

# macOS: sign the apps "ad hoc", after everything is inside them. Apple
# silicon Macs call a downloaded app with no valid signature "damaged" and
# refuse to open it. An ad-hoc signature is valid without an Apple developer
# account; users then right-click > Open once ("unidentified developer").
# The linker signs the programs, but that signature does not cover the
# bundle's resources added here, so it would not count. Inner app first.
if(APPLE)
    install(CODE [[
        foreach(_app "lantern_editor.app/Contents/Resources/lantern/runtime/lantern_player.app" "lantern_editor.app")
            execute_process(COMMAND /usr/bin/codesign --force --deep --sign - "${CMAKE_INSTALL_PREFIX}/${_app}"
                            RESULT_VARIABLE _signed)
            if(NOT _signed EQUAL 0)
                message(FATAL_ERROR "Could not sign ${CMAKE_INSTALL_PREFIX}/${_app}")
            endif()
        endforeach()
        execute_process(COMMAND /usr/bin/codesign --verify --deep --strict "${CMAKE_INSTALL_PREFIX}/lantern_editor.app"
                        RESULT_VARIABLE _valid)
        if(NOT _valid EQUAL 0)
            message(FATAL_ERROR "The signature of lantern_editor.app does not verify")
        endif()
        message(STATUS "Signed lantern_editor.app (ad hoc)")
    ]] COMPONENT lantern)
endif()

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

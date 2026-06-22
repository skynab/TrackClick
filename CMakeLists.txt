cmake_minimum_required(VERSION 3.16)
project(TrackClick VERSION 0.9.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt version detection ──────────────────────────────────────────────────────
# Prefer Qt6; fall back to Qt5 (5.12+) if Qt6 is not available.
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
if(Qt6_FOUND)
    set(QT Qt6)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)
    find_package(Qt6 OPTIONAL_COMPONENTS Multimedia LinguistTools QUIET)
    set(LINGUIST_FOUND ${Qt6LinguistTools_FOUND})
    set(MULTIMEDIA_FOUND ${Qt6Multimedia_FOUND})
    message(STATUS "TrackClick: building with Qt6 ${Qt6_VERSION}")
else()
    find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets Svg)
    set(QT Qt5)
    find_package(Qt5 OPTIONAL_COMPONENTS Multimedia LinguistTools QUIET)
    set(LINGUIST_FOUND ${Qt5LinguistTools_FOUND})
    set(MULTIMEDIA_FOUND ${Qt5Multimedia_FOUND})
    message(STATUS "TrackClick: building with Qt5 ${Qt5_VERSION}")
endif()

set(SOURCES
    main.cpp
    mainwindow.cpp
    dwellmanager.cpp
    clickinjector.cpp
    settingsdialog.cpp
    audioclicklistener.cpp
    translations/tsparser.cpp
    resources.qrc
)

set(HEADERS
    mainwindow.h
    dwellmanager.h
    clickinjector.h
    settingsdialog.h
    audioclicklistener.h
)

if(WIN32)
    if(NOT DEFINED BUILD_NUMBER)
        set(BUILD_NUMBER 0)
    endif()
    configure_file(app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
    add_executable(TrackClick WIN32 ${SOURCES} ${HEADERS} "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
elseif(APPLE)
    set(MACOSX_ICON icons/TrackClick.icns)
    set_source_files_properties(${MACOSX_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    add_executable(TrackClick MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${MACOSX_ICON})
    set_target_properties(TrackClick PROPERTIES
        MACOSX_BUNDLE_ICON_FILE TrackClick.icns)
else()
    add_executable(TrackClick ${SOURCES} ${HEADERS})
endif()

target_link_libraries(TrackClick PRIVATE
    ${QT}::Core
    ${QT}::Gui
    ${QT}::Widgets
    ${QT}::Svg
)

if(DEFINED BUILD_NUMBER)
    target_compile_definitions(TrackClick PRIVATE BUILD_NUMBER=${BUILD_NUMBER})
endif()

if(MULTIMEDIA_FOUND)
    target_link_libraries(TrackClick PRIVATE ${QT}::Multimedia)
    target_compile_definitions(TrackClick PRIVATE HAVE_MULTIMEDIA)
    message(STATUS "TrackClick: audio support enabled")
else()
    message(STATUS "TrackClick: audio support disabled (Qt Multimedia not found)")
endif()

if(WIN32)
    target_link_libraries(TrackClick PRIVATE user32)
    target_compile_definitions(TrackClick PRIVATE PLATFORM_WINDOWS)
elseif(APPLE)
    find_library(APP_SERVICES ApplicationServices REQUIRED)
    find_library(APPKIT AppKit REQUIRED)
    target_sources(TrackClick PRIVATE macos_utils.mm)
    target_link_libraries(TrackClick PRIVATE ${APP_SERVICES} ${APPKIT})
    target_compile_definitions(TrackClick PRIVATE PLATFORM_MACOS)
elseif(UNIX)
    find_package(X11 REQUIRED)
    if(NOT X11_Xtst_FOUND)
        message(FATAL_ERROR "libXtst not found. Install libxtst-dev (Debian/Ubuntu) or libXtst-devel (Fedora).")
    endif()
    target_link_libraries(TrackClick PRIVATE X11::X11 X11::Xtst)
    if(X11_Xi_FOUND)
        target_link_libraries(TrackClick PRIVATE X11::Xi)
        target_compile_definitions(TrackClick PRIVATE HAVE_XI2)
        message(STATUS "TrackClick: XInput2 raw motion tracking enabled (Wayland dwell fix)")
    else()
        message(STATUS "TrackClick: XInput2 not found — install libxi-dev for accurate dwell tracking on Wayland")
    endif()
    target_compile_definitions(TrackClick PRIVATE PLATFORM_LINUX)
endif()

install(TARGETS TrackClick DESTINATION bin)

# ── Linux: desktop integration + input-device access rule ─────────────────────
if(UNIX AND NOT APPLE)
    # udev rule granting the active session read access to pointer devices so the
    # dwell timer can track the cursor over every window on Wayland.  Shipped in
    # the .deb (the postinst reloads udev); harmless when bundled in the AppImage.
    install(FILES installer/71-trackclick-input.rules
            DESTINATION lib/udev/rules.d)
    install(FILES installer/TrackClick.desktop
            DESTINATION share/applications)
    install(FILES icons/app.svg
            DESTINATION share/icons/hicolor/scalable/apps
            RENAME TrackClick.svg)

    # The binary installs as "TrackClick" (matching the macOS/Windows target and
    # the desktop launcher's Exec=/StartupWMClass=), but the Debian package and
    # the command users type are lowercase "trackclick".  Ship a relative
    # lowercase symlink so `trackclick` works from a terminal too; the .desktop
    # launcher keeps using the capitalised binary.
    install(CODE "
        execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
            TrackClick \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/trackclick\")
    ")
endif()

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Translations ──────────────────────────────────────────────────────────────
set(TS_FILES
    translations/trackclick_cs.ts
    translations/trackclick_fr.ts
    translations/trackclick_es.ts
    translations/trackclick_zh_CN.ts
    translations/trackclick_ja.ts
    translations/trackclick_ko.ts
)

# .ts files are embedded in resources.qrc and parsed at runtime by TsTranslator
# — no lrelease required for the build.
# If LinguistTools ARE available, also compile .qm files for faster startup.
if(LINGUIST_FOUND)
    if(Qt6_FOUND)
        qt_add_lrelease(TrackClick
            TS_FILES ${TS_FILES}
            QM_FILES_OUTPUT_VARIABLE QM_FILES
        )
    else()
        qt5_add_translation(QM_FILES ${TS_FILES})
        add_custom_target(TrackClick_translations DEPENDS ${QM_FILES})
        add_dependencies(TrackClick TrackClick_translations)
    endif()

    add_custom_command(TARGET TrackClick POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TrackClick>/translations"
        VERBATIM
    )
    foreach(QM_FILE IN LISTS QM_FILES)
        add_custom_command(TARGET TrackClick POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QM_FILE}" "$<TARGET_FILE_DIR:TrackClick>/translations/"
            VERBATIM
        )
    endforeach()
    install(FILES ${QM_FILES} DESTINATION bin/translations)
endif()

# ── Packaging: Debian/Ubuntu .deb ─────────────────────────────────────────────
# The .deb installs to /usr and ships the udev rule, so input-device access is
# set up automatically at install time (the postinst reloads udev) and the
# on-screen keyboard is pulled in via Recommends — no terminal steps needed.
# The AppImage (built separately in CI) remains the portable cross-distro option.
if(UNIX AND NOT APPLE)
    set(CPACK_PACKAGE_NAME "trackclick")
    set(CPACK_PACKAGE_VENDOR "TrackClick")
    set(CPACK_PACKAGE_CONTACT "TrackClick <noreply@trackclick.app>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Dwell-click accessibility tool")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_GENERATOR "DEB")

    # Reload udev after install so the input-access rule applies immediately.
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/postinst")

    # Auto-derive linked-library dependencies from the binary; recommend the
    # on-screen keyboard and polkit (pkexec) which are used at runtime but not
    # captured by shared-library scanning.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "onboard, policykit-1")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

    include(CPack)
endif()

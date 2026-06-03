cmake_minimum_required(VERSION 3.16)
project(TrackClick VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 OPTIONAL_COMPONENTS LinguistTools QUIET)

set(SOURCES
    main.cpp
    mainwindow.cpp
    dwellmanager.cpp
    clickinjector.cpp
    settingsdialog.cpp
    translations/tsparser.cpp
    resources.qrc
)

set(HEADERS
    mainwindow.h
    dwellmanager.h
    clickinjector.h
    settingsdialog.h
)

if(WIN32)
    add_executable(TrackClick WIN32 ${SOURCES} ${HEADERS})
else()
    add_executable(TrackClick ${SOURCES} ${HEADERS})
endif()

target_link_libraries(TrackClick PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

if(WIN32)
    target_link_libraries(TrackClick PRIVATE user32)
    target_compile_definitions(TrackClick PRIVATE PLATFORM_WINDOWS)
elseif(APPLE)
    find_library(APP_SERVICES ApplicationServices REQUIRED)
    target_link_libraries(TrackClick PRIVATE ${APP_SERVICES})
    target_compile_definitions(TrackClick PRIVATE PLATFORM_MACOS)
elseif(UNIX)
    find_package(X11 REQUIRED)
    if(NOT X11_Xtst_FOUND)
        message(FATAL_ERROR "libXtst not found. Install libxtst-dev (Debian/Ubuntu) or libXtst-devel (Fedora).")
    endif()
    target_link_libraries(TrackClick PRIVATE X11::X11 X11::Xtst)
    target_compile_definitions(TrackClick PRIVATE PLATFORM_LINUX)
endif()

install(TARGETS TrackClick DESTINATION bin)

# ── Translations ──────────────────────────────────────────────────────────────
set(TS_FILES
    translations/trackclick_fr.ts
    translations/trackclick_es.ts
    translations/trackclick_zh_CN.ts
    translations/trackclick_ja.ts
    translations/trackclick_ko.ts
)

# .ts files are embedded in resources.qrc and parsed at runtime by TsTranslator
# — no lrelease required for the build.
# If LinguistTools ARE available, also compile .qm files for faster startup.
if(Qt6LinguistTools_FOUND)
    qt_add_lrelease(TrackClick
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE QM_FILES
    )
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

cmake_minimum_required(VERSION 3.16)
project(PointNClick VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    main.cpp
    mainwindow.cpp
    dwellmanager.cpp
    clickinjector.cpp
    settingsdialog.cpp
)

set(HEADERS
    mainwindow.h
    dwellmanager.h
    clickinjector.h
    settingsdialog.h
)

if(WIN32)
    add_executable(PointNClick WIN32 ${SOURCES} ${HEADERS})
else()
    add_executable(PointNClick ${SOURCES} ${HEADERS})
endif()

target_link_libraries(PointNClick PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

if(WIN32)
    target_link_libraries(PointNClick PRIVATE user32)
    target_compile_definitions(PointNClick PRIVATE PLATFORM_WINDOWS)
elseif(APPLE)
    find_library(APP_SERVICES ApplicationServices REQUIRED)
    target_link_libraries(PointNClick PRIVATE ${APP_SERVICES})
    target_compile_definitions(PointNClick PRIVATE PLATFORM_MACOS)
elseif(UNIX)
    find_package(X11 REQUIRED)
    if(NOT X11_Xtst_FOUND)
        message(FATAL_ERROR "libXtst not found. Install libxtst-dev (Debian/Ubuntu) or libXtst-devel (Fedora).")
    endif()
    target_link_libraries(PointNClick PRIVATE X11::X11 X11::Xtst)
    target_compile_definitions(PointNClick PRIVATE PLATFORM_LINUX)
endif()

install(TARGETS PointNClick DESTINATION bin)

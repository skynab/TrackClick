cmake_minimum_required(VERSION 3.16)

# Reuse the Qt version already detected by the parent.
find_package(${QT} REQUIRED COMPONENTS Core Gui Test)

add_executable(tst_dwellmanager
    tst_dwellmanager.cpp
    ${CMAKE_SOURCE_DIR}/dwellmanager.cpp
    ${CMAKE_SOURCE_DIR}/clickinjector.cpp
)

target_include_directories(tst_dwellmanager PRIVATE ${CMAKE_SOURCE_DIR})

target_compile_definitions(tst_dwellmanager PRIVATE TRACKCLICK_TESTING)

target_link_libraries(tst_dwellmanager PRIVATE
    ${QT}::Core
    ${QT}::Gui
    ${QT}::Test
)

# Mirror the platform definitions and libraries from the main build so that
# clickinjector.cpp compiles correctly (tests don't call the real platform
# functions, but the code still needs to compile).
if(WIN32)
    target_compile_definitions(tst_dwellmanager PRIVATE PLATFORM_WINDOWS)
    target_link_libraries(tst_dwellmanager PRIVATE user32)
elseif(APPLE)
    target_compile_definitions(tst_dwellmanager PRIVATE PLATFORM_MACOS)
    find_library(APP_SERVICES ApplicationServices REQUIRED)
    target_link_libraries(tst_dwellmanager PRIVATE ${APP_SERVICES})
elseif(UNIX)
    target_compile_definitions(tst_dwellmanager PRIVATE PLATFORM_LINUX)
    find_package(X11 REQUIRED)
    target_link_libraries(tst_dwellmanager PRIVATE X11::X11 X11::Xtst)
endif()

add_test(NAME DwellManager COMMAND tst_dwellmanager -v2)

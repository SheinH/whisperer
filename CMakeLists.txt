cmake_minimum_required(VERSION 3.20)

project(whisperer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)


set(HOMEBREW_PREFIX "/usr/local"
        CACHE PATH "Path to Homebrew installation")
set(CMAKE_CXX_COMPILER "/opt/homebrew/opt/llvm/bin/clang++")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${HOMEBREW_PREFIX}" "/opt/homebrew/opt/curl/")
set(CMAKE_MACOSX_RPATH ON)
if (NOT DEFINED ENV{PKG_CONFIG_PATH})
    set(ENV{PKG_CONFIG_PATH} "$ENV{HOME}/local/lib/pkgconfig")
else ()
    list(APPEND ENV{PKG_CONFIG_PATH} "$ENV{HOME}/local/lib/pkgconfig")
endif ()
list(APPEND ENV{PKG_CONFIG_PATH} "/opt/homebrew/opt/curl/lib/pkgconfig")

execute_process(COMMAND brew --prefix curl OUTPUT_VARIABLE CURL_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
if (CURL_PREFIX)
    list(APPEND CMAKE_PREFIX_PATH "${CURL_PREFIX}")
    set(CURL_ROOT "${CURL_PREFIX}")
    set(ENV{PKG_CONFIG_PATH} "${CURL_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()
list(APPEND CMAKE_IGNORE_PATH "/usr/local")


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network WebSockets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network WebSockets)
find_package(QHotkey CONFIG REQUIRED)
find_package(CURL REQUIRED)
if (APPLE)
    find_library(CORE_GRAPHICS_LIBRARY CoreGraphics)
    find_library(CORE_FOUNDATION_LIBRARY CoreFoundation)
    find_library(APPLICATION_SERVICES_LIBRARY ApplicationServices)
    set(PLATFORM_SOURCES keystrokes_mac.cpp)
    set(PLATFORM_LIBRARIES
            ${CORE_GRAPHICS_LIBRARY}
            ${CORE_FOUNDATION_LIBRARY}
            ${APPLICATION_SERVICES_LIBRARY}
    )
elseif (WIN32)
    set(PLATFORM_SOURCES keystrokes_win.cpp)
    set(PLATFORM_LIBRARIES user32)
else ()
    message(FATAL_ERROR "No keystroke implementation for this platform")
endif ()
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat
        libavfilter
        libavdevice
        libswresample
        libswscale
        libavutil
)


set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        audiorecorder.h
        audiorecorder.cpp
        transcriptionapi.h
        transcriptionapi.cpp
        keystrokes.cpp
        keystrokes.h
        StreamingAudioUploader.cpp
        StreamingAudioUploader.h
        AudioThread.cpp
        AudioThread.h
        ${PLATFORM_SOURCES}
)

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(whisperer
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
    )
    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET whisperer APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else ()
    if (ANDROID)
        add_library(whisperer SHARED
                ${PROJECT_SOURCES}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else ()
        add_executable(whisperer
                ${PROJECT_SOURCES}
        )
    endif ()
endif ()

target_link_libraries(whisperer PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::WebSockets
        ${PLATFORM_LIBRARIES}
        qhotkey
        PRIVATE PkgConfig::FFMPEG
        CURL::libcurl
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if (${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.whisperer)
endif ()
set_target_properties(whisperer PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
#        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS whisperer
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(whisperer)
endif ()

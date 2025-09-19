cmake_minimum_required(VERSION 3.16)

project(QtMediaPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/ffmpeg")
include_directories(${FFMPEG_ROOT}/include)
link_directories(${FFMPEG_ROOT}/lib)

set(SDL_ROOT "${CMAKE_SOURCE_DIR}/SDL3")
include_directories(${SDL_ROOT}/include)
link_directories(${SDL_ROOT}/lib)

INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGLWidgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

    qt_add_executable(QtMediaPlayer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        MediaReaderThread.h MediaReaderThread.cpp
        openglwidget.h openglwidget.cpp


        AudioDecoderThread.h AudioDecoderThread.cpp

        VideoDecoderThread.h VideoDecoderThread.cpp





    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET QtMediaPlayer APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(QtMediaPlayer SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(QtMediaPlayer
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# 导入 FFmpeg 库
set(FFMPEG_LIBS avcodec avformat avutil swscale swresample avfilter)
foreach(lib ${FFMPEG_LIBS})
    add_library(ffmpeg_${lib} SHARED IMPORTED)
    set_target_properties(ffmpeg_${lib} PROPERTIES
        IMPORTED_IMPLIB "${FFMPEG_ROOT}/lib/${lib}.lib"
    )
endforeach()




target_link_libraries(QtMediaPlayer PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    ffmpeg_avcodec
    ffmpeg_avformat
    ffmpeg_avutil
    ffmpeg_swscale
    ffmpeg_swresample
    ffmpeg_avfilter
    libSDL3.dll)
target_link_libraries(QtMediaPlayer PRIVATE Qt6::Core)
target_link_libraries(QtMediaPlayer PRIVATE Qt6::Core)


file(GLOB DLLS
    "${FFMPEG_ROOT}/bin/*.dll"
    "${SDL_ROOT}/bin/*.dll"

)

# 复制 DLL 到输出目录
add_custom_command(TARGET QtMediaPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DLLS}
    $<TARGET_FILE_DIR:QtMediaPlayer>
)


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.QtMediaPlayer)
endif()
set_target_properties(QtMediaPlayer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS QtMediaPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QtMediaPlayer)
endif()



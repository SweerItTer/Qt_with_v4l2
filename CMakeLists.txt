cmake_minimum_required(VERSION 3.5)

project(Test VERSION 0.1 LANGUAGES CXX)

# 是否使用交叉编译工具链的开关
if (0)
    message(STATUS "Using cross-compilation toolchain.")
    add_definitions(-DRV1126)

    # 使用工具链文件
    include(CMakeForceCompiler) # CMake 3.6 版本之前
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain.cmake)

    # 确保工具链文件被正确加载
    if(NOT CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "CMake is not in cross-compiling mode. Please check your toolchain file.")
    endif()

    # 设置库路径
    link_directories(${CMAKE_SYSROOT}/usr/lib)
    
else()
    message(STATUS "Not using cross-compilation toolchain.")
    include_directories(/usr/include/turbojpeg)
endif()
include_directories(/opt/libyuv/include/)


set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_QML_GENERATE_QMLLS_INI ON)
find_package(Qt5 COMPONENTS Quick Qml REQUIRED)

set(PROJECT_SOURCES
        main.cpp # 入口

        queue_.h # 安全队列
        mainwindow.cpp  # 主窗口
        mainwindow.h
        v4l2_video.cpp  # v4l2主要逻辑
        v4l2_video.h

        VideoProvider.cpp   # 视频流提供器
        VideoProvider.h

        Main.qml
        qml.qrc # 资源文件
)

add_executable(QC_e
    ${PROJECT_SOURCES}
)

target_link_libraries(QC_e PRIVATE 
    Qt5::Core
    Qt5::Quick
    Qt5::Qml
    turbojpeg 
    pthread 
    -l:libyuv.a)


cmake_minimum_required(VERSION 3.5)

project(Test VERSION 0.1 LANGUAGES CXX)

# 是否使用交叉编译工具链的开关
if (1)
    message(STATUS "Using cross-compilation toolchain.")

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
endif()



set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(Qt5_DIR /opt/Qt5.12.12/5.12.12/gcc_64/lib/cmake/Qt5)
find_package(Qt5 COMPONENTS Widgets REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        v4l2_video.cpp
        v4l2_video.h
        v4l2_thread.cpp
        rec.qrc
)

add_executable(QC_e
    ${PROJECT_SOURCES}
)

target_link_libraries(QC_e PRIVATE Qt5::Widgets turbojpeg)


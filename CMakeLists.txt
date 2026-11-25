cmake_minimum_required(VERSION 3.16)

project(qt-kylin-pam-demo LANGUAGES CXX)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent)

add_executable(qt-kylin-pam-demo src/main.cpp src/authentication.cpp)
target_compile_features(qt-kylin-pam-demo PRIVATE cxx_std_17)
target_link_libraries(qt-kylin-pam-demo PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_path(PAM_INCLUDE_DIR NAMES security/pam_appl.h)
    find_library(PAM_LIBRARY NAMES pam)

    if(NOT PAM_INCLUDE_DIR OR NOT PAM_LIBRARY)
        message(FATAL_ERROR "缺少 Linux-PAM 开发库，请安装目标系统提供的 PAM 开发包。")
    endif()

    target_include_directories(qt-kylin-pam-demo PRIVATE "${PAM_INCLUDE_DIR}")
    target_link_libraries(qt-kylin-pam-demo PRIVATE "${PAM_LIBRARY}")
endif()

cmake_minimum_required(VERSION 3.20)

project(RunBox)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(RunBox
    src/main.cpp
)

target_link_libraries(RunBox
    PRIVATE
    Qt6::Widgets
    Qt6::Network
)
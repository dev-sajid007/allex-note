cmake_minimum_required(VERSION 3.20)
project(allex-notes VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

add_executable(allex-notes
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.hpp
    src/note.cpp
    src/note.hpp
    src/notemanager.cpp
    src/notemanager.hpp
    resources/resources.qrc
)

target_link_libraries(allex-notes PRIVATE Qt6::Widgets Qt6::PrintSupport)

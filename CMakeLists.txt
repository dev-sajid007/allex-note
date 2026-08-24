cmake_minimum_required(VERSION 3.20)
project(allex-notes VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Network NetworkAuth)
find_package(OpenSSL REQUIRED)

set(ALLEX_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.hpp
    src/note.cpp
    src/note.hpp
    src/notemanager.cpp
    src/notemanager.hpp
    src/authmanager.cpp
    src/authmanager.hpp
    src/syncmanager.cpp
    src/syncmanager.hpp
    src/crypto.cpp
    src/crypto.hpp
    resources/resources.qrc
)

add_executable(allex-notes ${ALLEX_SOURCES})

target_link_libraries(allex-notes PRIVATE
    Qt6::Widgets Qt6::PrintSupport Qt6::Network Qt6::NetworkAuth
    OpenSSL::Crypto
)

# Install
include(GNUInstallDirs)

install(TARGETS allex-notes DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES allex-notes.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/icons/allex-notes-128.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
        RENAME allex-notes.png)
install(FILES assets/icons/allex-notes-64.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps
        RENAME allex-notes.png)
install(FILES assets/icons/allex-notes-48.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps
        RENAME allex-notes.png)
install(FILES assets/icons/allex-notes-32.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps
        RENAME allex-notes.png)
install(FILES assets/icons/allex-notes-16.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps
        RENAME allex-notes.png)

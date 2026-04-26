cmake_minimum_required(VERSION 3.16)
project(chesseycpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)

add_executable(
    chesseycpp
    src/main.cpp
    src/board.cpp
    src/moves.cpp
    src/engine.cpp
)
target_link_libraries(chesseycpp SFML::Graphics SFML::Window SFML::System SFML::Audio)

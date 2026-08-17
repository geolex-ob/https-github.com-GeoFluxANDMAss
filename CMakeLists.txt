cmake_minimum_required(VERSION 3.10)

project(GeoFluxMass LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets Core REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    mainwindow.cpp
    tool.cpp
    toolmodel.cpp
    wellconstruction.cpp
    circulation.cpp
    wellvisualization.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets Qt5::Core)

cmake_minimum_required(VERSION 3.16)
project(GTICalculator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(GTICalculator
    main.cpp
    database.cpp
    calculations.cpp
    mainwindow.cpp
    referencetoolsdialog.cpp
    referencecasingsdialog.cpp
)

target_link_libraries(GTICalculator PRIVATE Qt5::Widgets)
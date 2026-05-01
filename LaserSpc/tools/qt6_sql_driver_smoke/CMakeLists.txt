cmake_minimum_required(VERSION 3.21)
project(Qt6SqlDriverSmoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql)

qt_standard_project_setup()

qt_add_executable(Qt6SqlDriverSmoke
    main.cpp
)

target_link_libraries(Qt6SqlDriverSmoke PRIVATE Qt6::Core Qt6::Sql)

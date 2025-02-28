cmake_minimum_required(VERSION 3.31)

project(postgre_integration)

set(CMAKE_CXX_STANDARD 23)

find_package(ODBC REQUIRED)

add_executable(postgre_integration src/main.cpp)

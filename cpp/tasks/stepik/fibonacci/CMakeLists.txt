cmake_minimum_required(VERSION 3.14)
project(fibonacci)

add_subdirectory(tests)

set(fibonacci_src src/fibonacci.cpp)
set(fibonacci_include include/fibonacci.h)

add_library(fibonacci STATIC ${fibonacci_src} ${fibonacci_include})
target_include_directories(fibonacci PUBLIC ${fibonacci_include})
cmake_minimum_required(VERSION 3.14)

project(fibonacci_tests)

set(fibonacci_tests_src 
	src/main.cpp
	src/fibonacci_tests.cpp)

add_executable(fibonacci_tests ${fibonacci_tests_src})
target_link_libraries(fibonacci_tests fibonacci gtest_main)
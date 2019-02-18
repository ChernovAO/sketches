cmake_minimum_required(VERSION 3.5)

project(two_sum)
add_library(two_sum two_sum.cpp)

add_executable(two_sum_tests two_sum_tests.cpp)
target_link_libraries(two_sum_tests libgtest.a two_sum)

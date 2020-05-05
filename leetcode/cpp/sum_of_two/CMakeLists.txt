cmake_minimum_required(VERSION 3.14)

include(GoogleTest)

set(target_name sum_of_two)
set(target_src src/sum_of_two.cpp)

add_library(${target_name} STATIC ${target_src})
target_include_directories(${target_name} PUBLIC ./include)

set(test_src tests/main.cpp tests/sum_of_two_tests.cpp)

set(test_name ${target_name}_test)

add_executable(${test_name} ${test_src})
target_link_libraries(${test_name} gtest_main ${target_name}) 
gtest_add_tests(TARGET ${test_name})

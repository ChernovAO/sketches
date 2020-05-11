cmake_minimum_required(VERSION 3.14)

include(GoogleTest)

set(target_name zig_zag_conv)

file(GLOB target_src src/*.cpp)

add_library(${target_name} STATIC ${target_src})
target_include_directories(${target_name} PUBLIC ./include)

file(GLOB test_src tests/*.cpp)

set(test_name ${target_name}_test)

add_executable(${test_name} ${test_src})
target_link_libraries(${test_name} gtest_main ${target_name}) 

gtest_add_tests(TARGET ${test_name})


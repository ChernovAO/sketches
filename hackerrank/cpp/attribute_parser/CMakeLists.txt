cmake_minimum_required(VERSION 3.14)

include(GoogleTest)

set(target_name attribute_parser)

add_subdirectory(lib)
add_subdirectory(app)

file(GLOB test_src tests/*.cpp)

set(test_name ${target_name}_test)

add_executable(${test_name} ${test_src})
target_link_libraries(${test_name} PRIVATE gtest_main ${target_name}_lib) 

gtest_add_tests(TARGET ${test_name})


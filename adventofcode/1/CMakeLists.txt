cmake_minimum_required (VERSION 3.6)

project (adventofcode_first)

add_executable(adventofcode_first first.cpp)

set_target_properties(adventofcode_first PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    )

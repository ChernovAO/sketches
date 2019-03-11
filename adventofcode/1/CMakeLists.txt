cmake_minimum_required (VERSION 3.6)

# this is bad way to specify compiler, but for sketches it is work well
set(CMAKE_C_COMPILER gcc-8)
set(CMAKE_CXX_COMPILER g++-8)

project (adventofcode_first)

add_executable(adventofcode_first first.cpp)

set_target_properties(adventofcode_first PROPERTIES
    CXX_STANDARD 17
    )

target_link_libraries(adventofcode_first stdc++fs)

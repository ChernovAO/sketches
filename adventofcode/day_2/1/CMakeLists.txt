cmake_minimum_required (VERSION 3.6)

# this is bad way to specify compiler, but for sketches it is work well
set(CMAKE_C_COMPILER gcc-8)
set(CMAKE_CXX_COMPILER g++-8)

set(target adventofcode_day_2_1)

project (${target})

add_executable(${target} second.cpp)

set_target_properties(${target} PROPERTIES
    CXX_STANDARD 17
    )

target_link_libraries(${target} stdc++fs)

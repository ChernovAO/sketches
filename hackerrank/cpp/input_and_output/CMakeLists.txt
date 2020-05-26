cmake_minimum_required(VERSION 3.14)

set(target_name input_and_output)
file(GLOB target_src src/*.cpp)

add_executable(${target_name} ${target_src})
target_include_directories(${target_name} PUBLIC ./include)

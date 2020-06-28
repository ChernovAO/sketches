cmake_minimum_required(VERSION 3.14)

set(lib_name ${target_name}_lib)

file(GLOB lib_src src/*.cpp)

add_library(${lib_name} STATIC ${lib_src})
target_include_directories(${lib_name} PUBLIC ./include)

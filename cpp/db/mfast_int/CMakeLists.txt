cmake_minimum_required(VERSION 3.14)
project(mfast_int)

set(target_src src/mfast_int.cpp)

add_library(mfast_int SHARED ${target_src})

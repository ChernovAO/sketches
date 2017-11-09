cmake_minimum_required (VERSION 2.8)

project (factorial)

include_directories("${PROJECT_SOURCE_DIR}/libfactorial")
add_subdirectory(libfactorial)

add_library(libfactorial, libfactorial/factorial.cpp)

#main executable of project
add_executable(factorial, factorial_main.cpp)

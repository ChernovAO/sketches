# CMakeList.txt : CMake project for vs_cmake_cheks, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("vs_cmake_cheks")

set (CMAKE_CXX_STANDARD 17)

set (MODULE_NAME "test_using")
add_executable (${MODULE_NAME} "src/main.cpp")
target_include_directories(${MODULE_NAME} PRIVATE ./include)

# TODO: Add tests and install targets if needed.

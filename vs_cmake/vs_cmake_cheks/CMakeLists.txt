# CMakeList.txt : CMake project for vs_cmake_cheks, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("vs_cmake_cheks")

set (CMAKE_CXX_STANDARD 17)

add_subdirectory(rvalue_vs_lvalue)
add_subdirectory(sv_substr_npos)

# TODO: Add tests and install targets if needed.

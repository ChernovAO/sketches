cmake_minimum_required(VERSION 3.14)

set(app_name ${target_name})

file(GLOB target_src src/*.cpp)

add_executable(${app_name} ${target_src})
target_link_libraries(${app_name} PRIVATE ${target_name}_lib)

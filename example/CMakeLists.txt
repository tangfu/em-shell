cmake_minimum_required(VERSION 2.8)
project(emshell-example)

find_package(emshell REQUIRED)

add_definitions("-g -Wall")
add_executable(example example.c)
target_link_libraries(example pthread emshell)

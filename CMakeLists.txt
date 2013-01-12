project (emshell)
cmake_minimum_required(VERSION 2.8.4)


add_definitions("-g -Wall")
add_subdirectory(src)


export(PACKAGE mylib)
export(TARGETS emshell FILE emshell-exports.cmake)
configure_file(emshell-config.cmake.in ${CMAKE_BINARY_DIR}/emshell-config.cmake)

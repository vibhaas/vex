cmake_minimum_required(VERSION 4.2)
project(vexc)

set(CMAKE_CXX_STANDARD 20)

add_executable(vexc src/main.cpp
        src/lexer/lexer.cpp
        include/lexer/lexer.h)

target_include_directories(vexc PRIVATE include)
target_compile_options(vexc PRIVATE -Wall -Wextra -Wpedantic -Werror)
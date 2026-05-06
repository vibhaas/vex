cmake_minimum_required(VERSION 4.2)
project(vexc)

set(CMAKE_CXX_STANDARD 20)

add_executable(vexc
        src/main.cpp
        src/lexer/Lexer.cpp
        include/lexer/TokenType.hpp
        include/lexer/Token.hpp
)

target_include_directories(vexc PRIVATE include)
target_compile_options(vexc PRIVATE -Wall -Wextra -Wpedantic -Werror)
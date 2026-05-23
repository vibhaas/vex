cmake_minimum_required(VERSION 4.2)
project(vexc)

set(CMAKE_CXX_STANDARD 20)

add_executable(vexc
        include/misc/Error_code.hpp
        src/main.cpp
        include/lexer/Token.hpp
        include/lexer/TokenType.hpp
        include/lexer/Lexer.hpp
        src/lexer/Lexer.cpp
        include/ast/Ast.hpp
        include/parser/Parser.hpp
        src/parser/Parser.cpp
        src/semantics/Semantics.cpp
        include/semantics/Semantics.hpp
)

target_include_directories(vexc PRIVATE include)
target_compile_options(vexc PRIVATE -Wall -Wextra -Wpedantic -Werror)
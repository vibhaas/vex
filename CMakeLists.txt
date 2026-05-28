cmake_minimum_required(VERSION 4.2)
project(vexc)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

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
        include/llvm-immediate/LLVMImmediate.hpp
        src/llvm-immediate/LLVMImmediate.cpp
        include/vex-ir/VexIR.hpp
        include/vex-ir-gen/VexIRGenerator.hpp
        src/vex-ir-gen/VexIRGenerator.cpp
)

target_include_directories(vexc PRIVATE include)
target_include_directories(vexc SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_options(vexc PRIVATE -Wall -Wextra -Wpedantic -Werror)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
llvm_map_components_to_libnames(llvm_libs support core irreader executionengine mcjit native)
target_link_libraries(vexc ${llvm_libs})

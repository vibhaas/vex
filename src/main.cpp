//
// Created by accord.
//

#include <iostream>
#include <fstream>
#include <vector>

#include "misc/Error_code.hpp"
#include "lexer/Token.hpp"
#include "lexer/Lexer.hpp"
#include "ast/Ast.hpp"
#include "parser/Parser.hpp"
#include "semantics/Semantics.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [options] <filename>\n";
        std::exit(get_error_code(ErrorCode::INCORRECT_USAGE));
    }

    bool verbose = false, extra_verbose = false, stop_tokens = false, stop_ast = false, stop_semantics = false, immediate_llvm = false,
    emit_ir = false, optimized_llvm = false;
    std::string filename;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose") verbose = true;
        else if (arg == "--extra-verbose") { verbose = extra_verbose = true; }
        else if (arg == "--tokens") stop_tokens = true;
        else if (arg == "--ast") stop_ast = true;
        else if (arg == "--semantics") stop_semantics = true;
        else if (arg == "--immediate-llvm") immediate_llvm = true;
        else if (arg == "--emit-ir") emit_ir = true;
        else if (arg == "--optimized-llvm") optimized_llvm = true;
        else if (arg[0] == '-') {
            std::cerr << "Unknown flag: " << arg << "\n";
            std::exit(get_error_code(ErrorCode::INCORRECT_USAGE));
        } else {
            if (!filename.empty()) {
                std::cerr << "Multiple input files provided: " << filename << " and " << arg << "!\n";
                std::exit(get_error_code(ErrorCode::INCORRECT_USAGE));
            }
            filename = arg;
        }
    }

    if (filename.empty()) {
        std::cerr << "No input filename provided!\n";
        std::exit(get_error_code(ErrorCode::INCORRECT_USAGE));
    }

    std::ifstream f(filename);
    if (!f.good()) {
        std::cerr << "Could not read input file " << filename << "!\n";
        std::exit(get_error_code(ErrorCode::INPUT_ERROR));
    }

    // 1. Lexical Analysis
    bool lexerError = false;
    std::vector<Token> tokens = Lexer::tokenize(f, lexerError);
    if (lexerError) {
        std::cerr << "Compilation stopped: Lexer error.\n";
        std::exit(get_error_code(ErrorCode::DATA_ERROR));
    }
    if (verbose) std::cout << "Lexing successful.\n";
    if (stop_tokens || extra_verbose) {
        for (const auto& token : tokens) std::cout << token.to_string() << " ";
        std::cout << "\n";
        if (stop_tokens) return 0;
    }

    // 2. Parsing
    Parser parser(std::move(tokens));
    parser.parse();
    if (parser.has_errors()) {
        std::cerr << "Compilation stopped: Parser error.\n" << parser.print_errors() << "\n";
        std::exit(get_error_code(ErrorCode::DATA_ERROR));
    }
    if (verbose) std::cout << "Parsing successful.\n";
    if (stop_ast || extra_verbose) {
        std::cout << parser.print_ast() << "\n";
        if (stop_ast) return 0;
    }

    // 3. Semantic Analysis
    SemanticAnalyzer sem(parser.take_ast());
    sem.analyze();
    if (sem.error()) {
        std::cerr << "Compilation stopped: Semantic error.\n";
        std::exit(get_error_code(ErrorCode::DATA_ERROR));
    }
    if (verbose) std::cout << "Semantic analysis successful.\n";
    if (stop_semantics || extra_verbose) {
        std::cout << sem.print_ast() << "\n";
        if (stop_semantics) return 0;
    }

    // 4. Backend Generation
    if (immediate_llvm) {
        if (verbose) std::cout << "Generating immediate LLVM IR...\n";
        // TODO: Pass semantic AST to immediate LLVM generator
        return 0;
    }

    if (verbose) std::cout << "Generating custom SSA IR...\n";
    // TODO: Pass semantic AST to Custom SSA IR generator

    if (emit_ir) {
        if (verbose) std::cout << "Emitting custom SSA IR...\n";
        // TODO: Print the custom SSA IR
        return 0;
    }

    if (verbose || optimized_llvm) std::cout << "Lowering SSA IR to optimized LLVM IR...\n";
    // TODO: Pass Custom SSA IR to LLVM lowering phase

    return 0;
}

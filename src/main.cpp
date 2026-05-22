//
// Created by accord.
//

#include <iostream>
#include <fstream>
#include <vector>

#include "misc/Error_code.hpp"
#include "lexer/Token.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Ast.hpp"
#include "parser/Parser.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        std::exit(get_error_code(ErrorCode::INCORRECT_USAGE));
    }

    std::ifstream f(argv[1]);
    if (!f.good()) {
        std::cerr << "Could not read " << argv[1] << " !" << std::endl;
        std::exit(get_error_code(ErrorCode::INPUT_ERROR));
    }

    bool lexerError = false;
    std::vector<Token> tokens = Lexer::tokenize(f, lexerError);
    if (lexerError) {
        std::cerr << "Compilation stopped : Lexer error." << std::endl;
        std::exit(get_error_code(ErrorCode::DATA_ERROR));
    }

    for (auto &token : tokens) {
        std::cout << token.to_string() << std::endl;
    }

    // now start parsing
    Parser parser(std::move(tokens));
    parser.parse();
    if (parser.has_errors()) {
        std::cerr << "Compilation stopped : Parser error." << std::endl;
        std::cerr << parser.print_errors() << std::endl;
        std::exit(get_error_code(ErrorCode::DATA_ERROR));
    }

    std::cout << "Parsing successful." << std::endl;
    std::cout << parser.print_ast() << std::endl;

    return 0;
}

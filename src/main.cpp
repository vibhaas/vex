//
// Created by accord.
//

#include <iostream>
#include <fstream>
#include <vector>

#include "lexer/Token.hpp"
#include "lexer/Lexer.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        std::exit(64);
    }

    std::ifstream f(argv[1]);
    if (!f.good()) {
        std::cerr << "Could not read " << argv[1] << " !" << std::endl;
        std::exit(66);
    }

    bool lexerError = false;
    std::vector<Token> tokens = Lexer::tokenize(f, lexerError);
    if (lexerError) {
        std::cerr << "Compilation stopped : Lexer error." << std::endl;
        std::exit(65);
    }

    for (auto &token : tokens) {
        std::cout << token.to_string() << std::endl;
    }

    return 0;
}

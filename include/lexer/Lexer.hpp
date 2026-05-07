//
// Created by accord.
//

#ifndef VEXC_LEXER_H
#define VEXC_LEXER_H

#include <vector>
#include <fstream>
#include "lexer/Token.hpp"


namespace Lexer {
    std::vector<Token> tokenize(std::ifstream& f);
}

#endif //VEXC_LEXER_H

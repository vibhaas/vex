//
// Created by accord.
//

#ifndef VEXC_TOKEN_H
#define VEXC_TOKEN_H

#include "lexer/TokenType.hpp"
#include <string>
#include <utility>

// general Token class

class Token {
public:
    TokenType type;
    std::string lexeme;
    int line, column;

    Token(const TokenType type, std::string lexeme, const int line, const int column)
        : type(type), lexeme(std::move(lexeme)), line(line), column(column) {}

    [[nodiscard]] std::string to_string() const {
        return "<" + token_type_to_string(type) + ", \"" + lexeme + "\", " + std::to_string(line) + ", " + std::to_string(column) + ">";
    }
};

#endif //VEXC_TOKEN_H

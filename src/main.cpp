//
// Created by accord.
//

#include <iostream>
#include "lexer/TokenType.hpp"
#include "lexer/Token.hpp"

int main() {
    const Token token(TokenType::IDENTIFIER, "fib", 10, 3);
    std::cout << token.to_string() << std::endl;

    return 0;
}

//
// Created by accord.
//

#include "lexer/Lexer.hpp"
#include "lexer/TokenType.hpp"
#include "lexer/Token.hpp"

#include <string>
#include <fstream>
#include <iostream>
#include <vector>

namespace Lexer {
    std::vector<Token> tokenize(std::ifstream& f) {
        std::vector<Token> tokens;
        std::string line;
        int row = 0;
        while (std::getline(f, line)) {
            const int len = static_cast<int>(line.length());
            for (int i = 0; i < len; i++) {
                if (line[i] == ' ') continue;
                if (line[i] == '{') {
                    tokens.emplace_back(TokenType::LEFT_CURLY_BRACKET, "", row, i);
                    continue;
                }
                if (line[i] == '}') {
                    tokens.emplace_back(TokenType::RIGHT_CURLY_BRACKET, "", row, i);
                    continue;
                }
                if (line[i] == '[') {
                    tokens.emplace_back(TokenType::LEFT_SQUARE_BRACKET, "", row, i);
                    continue;
                }
                if (line[i] == '[') {
                    tokens.emplace_back(TokenType::RIGHT_SQUARE_BRACKET, "", row, i);
                    continue;
                }
                if (line[i] == '(') {
                    tokens.emplace_back(TokenType::LEFT_PAREN, "", row, i);
                    continue;
                }
                if (line[i] == ')') {
                    tokens.emplace_back(TokenType::RIGHT_PAREN, "", row, i);
                    continue;
                }
                if (line[i] == ';') {
                    tokens.emplace_back(TokenType::SEMICOLON, "", row, i);
                    continue;
                }
                if (line[i] == ':') {
                    tokens.emplace_back(TokenType::COLON, "", row, i);
                    continue;
                }
                if (line[i] == '+') {
                    tokens.emplace_back(TokenType::PLUS, "", row, i);
                    continue;
                }
                if (line[i] == '*') {
                    tokens.emplace_back(TokenType::MULTIPLY, "", row, i);
                    continue;
                }
                if (line[i] == '/') {
                    tokens.emplace_back(TokenType::DIVIDE, "", row, i);
                    continue;
                }
                if (line[i] == '%') {
                    tokens.emplace_back(TokenType::MODULO, "", row, i);
                    continue;
                }
                
                //std::cout << line << std::endl;
            }
            row++;
        }
        tokens.emplace_back(TokenType::END_OF_FILE, "", row, 0);
        return tokens;
    }
}
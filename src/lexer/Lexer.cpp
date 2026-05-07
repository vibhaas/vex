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
#include <algorithm>
#include <cctype>

namespace Lexer {
    bool isWhitespace(const char c) {
        return (c == ' ' || c == '\t' || c == '\f' || c == '\r' || c == '\v' || c == '\n');
    }
    std::vector<Token> tokenize(std::ifstream& f, bool &lexerError) {
        std::vector<Token> tokens;
        std::string line;
        int row = 0;
        while (std::getline(f, line)) {
            const int len = static_cast<int>(line.length());
            for (int i = 0; i < len; i++) {
                if (line[i] == '#') break; // comments
                if (isWhitespace(line[i])) continue; // whitespace

                // single char symbols

                if (line[i] == '{') {
                    tokens.emplace_back(TokenType::LEFT_CURLY_BRACKET, "{", row, i);
                    continue;
                }
                if (line[i] == '}') {
                    tokens.emplace_back(TokenType::RIGHT_CURLY_BRACKET, "}", row, i);
                    continue;
                }
                if (line[i] == '[') {
                    tokens.emplace_back(TokenType::LEFT_SQUARE_BRACKET, "[", row, i);
                    continue;
                }
                if (line[i] == ']') {
                    tokens.emplace_back(TokenType::RIGHT_SQUARE_BRACKET, "]", row, i);
                    continue;
                }
                if (line[i] == '(') {
                    tokens.emplace_back(TokenType::LEFT_PAREN, "(", row, i);
                    continue;
                }
                if (line[i] == ')') {
                    tokens.emplace_back(TokenType::RIGHT_PAREN, ")", row, i);
                    continue;
                }
                if (line[i] == ';') {
                    tokens.emplace_back(TokenType::SEMICOLON, ";", row, i);
                    continue;
                }
                if (line[i] == ':') {
                    tokens.emplace_back(TokenType::COLON, ":", row, i);
                    continue;
                }
                if (line[i] == ',') {
                    tokens.emplace_back(TokenType::COMMA, ",", row, i);
                    continue;
                }
                if (line[i] == '+') {
                    tokens.emplace_back(TokenType::PLUS, "+", row, i);
                    continue;
                }
                if (line[i] == '*') {
                    tokens.emplace_back(TokenType::MULTIPLY, "*", row, i);
                    continue;
                }
                if (line[i] == '/') {
                    tokens.emplace_back(TokenType::DIVIDE, "/", row, i);
                    continue;
                }
                if (line[i] == '%') {
                    tokens.emplace_back(TokenType::MODULO, "%", row, i);
                    continue;
                }

                // strictly double char symbols

                if (line[i] == '.') {
                    if (i == len-1 || line[i+1] != '.') {
                        std::cerr << "Lexer: Unexpected symbol '.' on line " << row << " character " << i << ". Did you mean '..'? " << std::endl;
                        lexerError = true;
                        return tokens;
                    }
                    tokens.emplace_back(TokenType::DOUBLE_DOT, "..", row, i);
                    i++;
                    continue;
                }
                if (line[i] == '&') {
                    if (i == len-1 || line[i+1] != '&') {
                        std::cerr << "Lexer: Unexpected symbol '&' on line " << row << " character " << i << ". Did you mean '&&'? " << std::endl;
                        lexerError = true;
                        return tokens;
                    }
                    tokens.emplace_back(TokenType::AND, "&&", row, i);
                    i++;
                    continue;
                }
                if (line[i] == '|') {
                    if (i == len-1 || line[i+1] != '|') {
                        std::cerr << "Lexer: Unexpected symbol '|' on line " << row << " character " << i << ". Did you mean '||'? " << std::endl;
                        lexerError = true;
                        return tokens;
                    }
                    tokens.emplace_back(TokenType::OR, "||", row, i);
                    i++;
                    continue;
                }

                // ambiguous single vs double char symbols

                if (line[i] == '>') {
                    if (i == len-1 || line[i+1] != '=') {
                        tokens.emplace_back(TokenType::GREATER, ">", row, i);
                        continue;
                    }
                    tokens.emplace_back(TokenType::GREATER_EQUAL, ">=", row, i);
                    i++;
                    continue;
                }
                if (line[i] == '<') {
                    if (i == len-1 || line[i+1] != '=') {
                        tokens.emplace_back(TokenType::LESS, "<", row, i);
                        continue;
                    }
                    tokens.emplace_back(TokenType::LESS_EQUAL, "<=", row, i);
                    i++;
                    continue;
                }
                if (line[i] == '=') {
                    if (i == len-1 || line[i+1] != '=') {
                        tokens.emplace_back(TokenType::EQUAL, "=", row, i);
                        continue;
                    }
                    tokens.emplace_back(TokenType::EQUAL_EQUAL, "==", row, i);
                    i++;
                    continue;
                }
                if (line[i] == '!') {
                    if (i == len-1 || line[i+1] != '=') {
                        tokens.emplace_back(TokenType::NOT, "!", row, i);
                        continue;
                    }
                    tokens.emplace_back(TokenType::NOT_EQUAL, "!=", row, i);
                    i++;
                    continue;
                }
                if (line[i] == '-') {
                    if (i == len-1 || line[i+1] != '>') {
                        tokens.emplace_back(TokenType::MINUS, "-", row, i);
                        continue;
                    }
                    tokens.emplace_back(TokenType::ARROW, "->", row, i);
                    i++;
                    continue;
                }

                if (line[i] == '\"') {
                    // string opened, we have to handle carefully + handle escape characters
                    int j = i+1;
                    if (j == len) {
                        std::cerr << "Lexer: String not closed on line " << row << " character " << i << ". " << std::endl;
                        lexerError = true;
                        return tokens;
                    }
                    std::string body;
                    bool stopped = false;
                    for (; j < len; j++) {
                        if (line[j] == '\"') {
                            stopped = true; break;
                        }

                        // escape chars
                        if (line[j] == '\\' && j+1 < len) {
                            j++;
                            switch (line[j]) {
                                case 'n': body += '\n'; break;
                                case 'r': body += '\r'; break;
                                case 't': body += '\t'; break;
                                case '"': body += '"'; break;
                                case '\'': body += '\''; break;
                                case 'a': body += '\a'; break;
                                case 'b': body += '\b'; break;
                                case 'v': body += '\v'; break;
                                case 'f': body += '\f'; break;
                                case '\\': body += '\\'; break;
                                default:
                                    body += line[j];
                                break;
                            }
                            continue;
                        }

                        body += line[j];
                    }
                    if (!stopped) {
                        std::cerr << "Lexer: String not closed on line " << row << " character " << j-1 << ". " << std::endl;
                        lexerError = true;
                        return tokens;
                    }
                    tokens.emplace_back(TokenType::STRING, body, row, i);
                    i = j;
                    continue;
                }

                if (std::isdigit(static_cast<unsigned char>(line[i]))) {
                    // number opened
                    int j = i;
                    std::string number;
                    for (; j < len; j++) {
                        if (!std::isdigit(static_cast<unsigned char>(line[j]))) break;
                        number += line[j];
                    }
                    tokens.emplace_back(TokenType::NUMBER, number, row, i);
                    i = j-1;
                    continue;
                }

                if (std::isalpha(static_cast<unsigned char>(line[i]))) {
                    // identifier or keyword opened
                    int j = i;
                    std::string name;
                    for (; j < len; j++) {
                        if (!(std::isalpha(static_cast<unsigned char>(line[j])) || std::isdigit(static_cast<unsigned char>(line[j])) || line[j] == '_')) break;
                        name += line[j];
                    }
                    // keywords
                    std::string temp = name;
                    std::ranges::transform(temp, temp.begin(),
                                           [](const unsigned char c){ return std::tolower(static_cast<unsigned char>(c)); });
                    if (temp == "i32") tokens.emplace_back(TokenType::INT32, "i32", row, i);
                    else if (temp == "bool") tokens.emplace_back(TokenType::BOOL, "bool", row, i);
                    else if (temp == "nil") tokens.emplace_back(TokenType::NIL, "nil", row, i);
                    else if (temp == "true") tokens.emplace_back(TokenType::TRUE, "true", row, i);
                    else if (temp == "false") tokens.emplace_back(TokenType::FALSE, "false", row, i);
                    else if (temp == "if") tokens.emplace_back(TokenType::IF, "if", row, i);
                    else if (temp == "elif") tokens.emplace_back(TokenType::ELIF, "elif", row, i);
                    else if (temp == "else") tokens.emplace_back(TokenType::ELSE, "else", row, i);
                    else if (temp == "break") tokens.emplace_back(TokenType::BREAK, "break", row, i);
                    else if (temp == "fun") tokens.emplace_back(TokenType::FUN, "fun", row, i);
                    else if (temp == "return") tokens.emplace_back(TokenType::RETURN, "return", row, i);
                    else if (temp == "exit") tokens.emplace_back(TokenType::EXIT, "exit", row, i);
                    else if (temp == "while") tokens.emplace_back(TokenType::WHILE, "while", row, i);
                    else if (temp == "for") tokens.emplace_back(TokenType::FOR, "for", row, i);
                    else if (temp == "in") tokens.emplace_back(TokenType::IN, "in", row, i);
                    else if (temp == "read") tokens.emplace_back(TokenType::READ, "read", row, i);
                    else if (temp == "print") tokens.emplace_back(TokenType::PRINT, "print", row, i);
                    else if (temp == "println") tokens.emplace_back(TokenType::PRINTLN, "println", row, i);
                    else tokens.emplace_back(TokenType::IDENTIFIER, name, row, i);
                    i = j-1;
                    continue;
                }
                // some unknown char
                lexerError = true;
                std::cerr << "Lexer: Unidentified symbol " << line[i] << " on line " << row << " character " << i << ". " << std::endl;
                return tokens;
            }
            row++;
        }
        tokens.emplace_back(TokenType::END_OF_FILE, "", row, 0);
        return tokens;
    }
}
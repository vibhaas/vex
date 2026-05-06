//
// Created by accord.
//

#ifndef VEXC_TOKENTYPE_H
#define VEXC_TOKENTYPE_H

#include <string>

// An enum for all 48 tokens in V1 of Vex

enum class TokenType {
    // Literals
    IDENTIFIER,
    NUMBER,
    STRING,

    // Single char tokens (mostly symbols)
    LEFT_CURLY_BRACKET, RIGHT_CURLY_BRACKET,
    LEFT_SQUARE_BRACKET, RIGHT_SQUARE_BRACKET,
    LEFT_PAREN, RIGHT_PAREN,
    SEMICOLON, COMMA,
    COLON, EQUAL, DOUBLE_DOT, ARROW,

    // One or two char tokens (mostly operators)
    GREATER, GREATER_EQUAL,
    LESS, LESS_EQUAL,
    EQUAL_EQUAL, NOT_EQUAL,
    PLUS, MINUS, MULTIPLY, DIVIDE,
    AND, OR, NOT, MODULO,

    // Keywords
    INT32, BOOL, NIL,
    TRUE, FALSE,
    IF, ELIF, ELSE, BREAK,
    FUN, RETURN, EXIT,
    WHILE, FOR, IN,
    READ, PRINT, PRINTLN,

    END_OF_FILE,
};

// Helper function to get the type
inline std::string token_type_to_string(const TokenType type) {
    switch (type) {
        case TokenType::IDENTIFIER:
            return "identifier";
        case TokenType::NUMBER:
            return "number";
        case TokenType::STRING:
            return "string";

        case TokenType::LEFT_CURLY_BRACKET:
            return "left-curly-bracket";
        case TokenType::RIGHT_CURLY_BRACKET:
            return "right-curly-bracket";
        case TokenType::LEFT_SQUARE_BRACKET:
            return "left-square-bracket";
        case TokenType::RIGHT_SQUARE_BRACKET:
            return "right-square-bracket";
        case TokenType::LEFT_PAREN:
            return "left-paren";
        case TokenType::RIGHT_PAREN:
            return "right-paren";
        case TokenType::SEMICOLON:
            return "semicolon";
        case TokenType::COMMA:
            return "comma";
        case TokenType::COLON:
            return "colon";
        case TokenType::EQUAL:
            return "equal";
        case TokenType::DOUBLE_DOT:
            return "double-dot";
        case TokenType::ARROW:
            return "arrow";

        case TokenType::GREATER:
            return "greater";
        case TokenType::GREATER_EQUAL:
            return "greater-equal";
        case TokenType::LESS:
            return "less";
        case TokenType::LESS_EQUAL:
            return "less-equal";
        case TokenType::EQUAL_EQUAL:
            return "equal-equal";
        case TokenType::NOT_EQUAL:
            return "not-equal";
        case TokenType::PLUS:
            return "plus";
        case TokenType::MINUS:
            return "minus";
        case TokenType::MULTIPLY:
            return "multiply";
        case TokenType::DIVIDE:
            return "divide";
        case TokenType::AND:
            return "and";
        case TokenType::OR:
            return "or";
        case TokenType::NOT:
            return "not";
        case TokenType::MODULO:
            return "modulo";

        case TokenType::INT32:
            return "int32";
        case TokenType::BOOL:
            return "bool";
        case TokenType::NIL:
            return "nil";
        case TokenType::TRUE:
            return "true";
        case TokenType::FALSE:
            return "false";
        case TokenType::IF:
            return "if";
        case TokenType::ELIF:
            return "elif";
        case TokenType::ELSE:
            return "else";
        case TokenType::BREAK:
            return "break";
        case TokenType::FUN:
            return "fun";
        case TokenType::RETURN:
            return "return";
        case TokenType::EXIT:
            return "exit";
        case TokenType::WHILE:
            return "while";
        case TokenType::FOR:
            return "for";
        case TokenType::IN:
            return "in";
        case TokenType::READ:
            return "read";
        case TokenType::PRINT:
            return "print";
        case TokenType::PRINTLN:
            return "println";

        case TokenType::END_OF_FILE:
            return "end-of-file";

        default:
            return "INVALID";
    }
}

#endif //VEXC_TOKENTYPE_H

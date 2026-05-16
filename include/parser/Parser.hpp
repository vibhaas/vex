//
// Created by accord.
//

#ifndef VEXC_PARSER_H
#define VEXC_PARSER_H
#include <vector>
#include <string>
#include "lexer/Token.hpp"
#include "parser/Ast.hpp"

struct ParseError {
    Token token;
    std::string message;
};

class Parser {
private:
    std::vector<Token> tokens;
    std::vector<AST::Stmt> AST;
    std::vector<ParseError> errors;
public:
    explicit Parser(std::vector<Token> p_tokens) : tokens(std::move(p_tokens)) {};
    void parse();
    bool has_errors();
    std::string print_errors();
    std::vector<AST::Stmt> get_ast();
    std::string print_ast();
};

#endif //VEXC_PARSER_H

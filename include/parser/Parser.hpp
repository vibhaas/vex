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
    int current = 0;
    std::vector<Token> tokens;
    std::vector<AST::Stmt> ast;
    std::vector<ParseError> errors;
public:
    explicit Parser(std::vector<Token> p_tokens);
    void parse();
    AST::Expr parse_expression();
    [[nodiscard]] bool has_errors() const;
    [[nodiscard]] std::string print_errors() const;
    [[nodiscard]] std::vector<AST::Stmt> get_ast();
    [[nodiscard]] std::string print_ast() const;
};

#endif //VEXC_PARSER_H

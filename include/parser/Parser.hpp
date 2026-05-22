//
// Created by accord.
//

#ifndef VEXC_PARSER_H
#define VEXC_PARSER_H
#include <vector>
#include <string>
#include "lexer/TokenType.hpp"
#include "lexer/Token.hpp"
#include "parser/Ast.hpp"

struct ParseError {
    Token token;
    std::string message;
    ParseError();
    ParseError(Token tok, std::string msg) : token(std::move(tok)), message(std::move(msg)) {};
};

class ParseException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Parser {
private:
    int current = 0;
    std::vector<Token> tokens;
    std::vector<AST::Stmt> ast;
    std::vector<ParseError> errors;

    [[nodiscard]] const Token& peek() const;
    [[nodiscard]] const Token& next_token() const;
    [[nodiscard]] const Token& previous_token() const;
    const Token& advance();
    [[nodiscard]] bool check(TokenType type) const;
    [[nodiscard]] bool is_at_end() const;
    [[nodiscard]] bool is_at_semi() const;
    bool match(std::initializer_list<TokenType> types);
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string &msg);
    void synchronize();

    void parse_statement();
    std::unique_ptr<AST::Expr> parse_expression();

public:
    explicit Parser(std::vector<Token> p_tokens);
    void parse();
    [[nodiscard]] bool has_errors() const;
    [[nodiscard]] std::string print_errors() const;
    [[nodiscard]] std::vector<AST::Stmt> get_ast();
    [[nodiscard]] std::string print_ast() const;
};

#endif //VEXC_PARSER_H

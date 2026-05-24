//
// Created by accord.
//

#ifndef VEXC_PARSER_H
#define VEXC_PARSER_H
#include <vector>
#include <string>
#include "lexer/TokenType.hpp"
#include "lexer/Token.hpp"
#include "ast/Ast.hpp"

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
    std::vector<std::unique_ptr<AST::Stmt>> ast;
    std::vector<ParseError> errors;

    [[nodiscard]] const Token& peek() const;
    [[nodiscard]] const Token& previous_token() const;
    const Token& advance();
    [[nodiscard]] bool check(TokenType type) const;
    [[nodiscard]] bool is_at_end() const;
    [[nodiscard]] bool is_at_semi() const;
    bool match(std::initializer_list<TokenType> types);
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string &msg);
    void synchronize();

    static int get_prefix_bp(TokenType type);
    static std::pair<int, int> get_infix_bp(TokenType type);
    std::unique_ptr<AST::Expr> parse_expr_bp(int min_bp);

    std::unique_ptr<AST::Stmt> parse_statement();
    std::unique_ptr<AST::Stmt> parse_block_stmt();
    AST::VarTypeSpec parse_var_type();
    std::unique_ptr<AST::Stmt> parse_function_stmt();
    std::unique_ptr<AST::Stmt> parse_variable_decl_stmt();
    std::unique_ptr<AST::Stmt> parse_io_stmt();
    std::unique_ptr<AST::Stmt> parse_expression_stmt();
    std::unique_ptr<AST::Stmt> parse_if_else_stmt();
    std::unique_ptr<AST::Stmt> parse_for_stmt();
    std::unique_ptr<AST::Stmt> parse_while_stmt();
    std::unique_ptr<AST::Stmt> parse_return_stmt();
    static std::unique_ptr<AST::Stmt> parse_break_stmt();
    static std::unique_ptr<AST::Stmt> parse_continue_stmt();
    std::unique_ptr<AST::Stmt> parse_exit_stmt();
public:
    explicit Parser(std::vector<Token> p_tokens);
    void parse();
    [[nodiscard]] bool has_errors() const;
    [[nodiscard]] std::string print_errors() const;
    [[nodiscard]] std::vector<std::unique_ptr<AST::Stmt>> take_ast();
    [[nodiscard]] std::string print_ast() const;
};

#endif //VEXC_PARSER_H

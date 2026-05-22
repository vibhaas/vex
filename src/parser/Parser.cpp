//
// Created by accord.
//

#include "parser/Parser.hpp"

#include <vector>
#include <string>
#include "lexer/Token.hpp"
#include "parser/Ast.hpp"

Parser::Parser(std::vector<Token> p_tokens) : tokens(std::move(p_tokens)) {};

// public functions

bool Parser::has_errors() const {
    return !errors.empty();
}
std::string Parser::print_errors() const {
    std::string value;
    for (const auto &error : errors) {
        value += "Error on token " + error.token.to_string() + " : " + error.message + "\n";
    }
    return value;
}
std::vector<AST::Stmt> Parser::get_ast()  {
    return std::move(ast);
}
std::string Parser::print_ast() const {
    std::string value;
    for (const auto &stmt : ast) {
        value += print_stmt(stmt);
        value += " END_STATEMENT; \n";
    }
    return value;
}

// helper functions now...

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::next_token() const {
    return tokens[current+1];
}

const Token& Parser::previous_token() const {
    return tokens[current-1];
}

const Token& Parser::advance() {
    if (!is_at_end()) current++;
    return previous_token();
}

bool Parser::check(const TokenType type) const {
    return peek().type == type;
}

bool Parser::is_at_end() const {
    return check(TokenType::END_OF_FILE);
}

bool Parser::is_at_semi() const {
    return check(TokenType::SEMICOLON);
}

bool Parser::match(const std::initializer_list<TokenType> types) {
    for (const auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

bool Parser::match(const TokenType type) {
    return Parser::match({type});
}

const Token& Parser::consume(const TokenType type, const std::string &msg) {
    if (check(type)) return advance();
    errors.emplace_back(peek(), msg);
    throw ParseException(msg);
}

void Parser::synchronize() {
    while (!is_at_end() && !is_at_semi()) advance();
}

// Gonna switch up and use Pratt Parsing (https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
// Because honestly, f*** the idea of sitting down and
// writing expression, subexpression, term, factor, atom, blah blah grammar rules
// when I can be sane
void Parser::parse() {
    // cool time to do stuff now
    // parse each statement at a time
    while (!is_at_end()) {
        try {
            parse_statement();
        }
        catch (const ParseException& e) {
            synchronize();
        }
    }
}

void Parser::parse_statement() {
    ast.emplace_back(parse_expression());
    if (is_at_semi()) advance();
}

using ExprNode = std::unique_ptr<AST::Expr>;
ExprNode Parser::parse_expression() {
    return {};
}
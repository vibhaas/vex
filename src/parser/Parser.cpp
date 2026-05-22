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
        value += std::string("Parser Error on token <") + token_type_to_string(error.token.type) + "> : \"" + error.token.lexeme
        + "\" on line " + std::to_string(error.token.line) + " character " + std::to_string(error.token.column) + " : "
        + error.message + "\n";
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
    throw ParseException(msg);
}

void Parser::synchronize() {
    while (!is_at_end() && !is_at_semi()) advance();
    if (is_at_semi()) advance();
}

void Parser::parse() {
    // cool time to do stuff now
    // parse each statement at a time
    while (!is_at_end()) {
        try {
            parse_statement();
        }
        catch (const ParseException& e) {
            errors.emplace_back(peek(), e.what());
            synchronize();
        }
    }
}

void Parser::parse_statement() {
    ast.emplace_back(parse_expr_bp(0));
    consume(TokenType::SEMICOLON, "Expected closing ';' for statement");
}

// Gonna switch up and use Pratt Parsing for expressions (https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
// Because honestly, f*** the idea of sitting down and
// writing expression, subexpression, term, factor, atom, blah blah grammar rules
// when I can be sane

int Parser::get_prefix_bp(const TokenType type) {
    switch(type) {
        case TokenType::MINUS:
        case TokenType::NOT:
            return 80;
        default:
           return -1;
    }
}

std::pair<int, int> Parser::get_infix_bp(const TokenType type) {
    switch(type) {
        case TokenType::MULTIPLY:
        case TokenType::DIVIDE:
        case TokenType::MODULO:
            return {70, 71};
        case TokenType::PLUS:
        case TokenType::MINUS:
            return {60, 61};
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
            return {50, 51};
        case TokenType::EQUAL_EQUAL:
        case TokenType::NOT_EQUAL:
            return {40, 41};
        case TokenType::AND:
            return {30, 31};
        case TokenType::OR:
            return {20, 21};
        case TokenType::EQUAL:
            return {11, 10}; // right associative
        default:
            return {-1, -1};
    }
}

std::unique_ptr<AST::Expr> Parser::parse_expr_bp(const int min_bp) {
    std::unique_ptr<AST::Expr> lhs;

    if (match(TokenType::LEFT_PAREN)) {
        // opens a '(' block
        auto inner_expr = parse_expr_bp(0);
        lhs = std::make_unique<AST::Expr>(AST::GroupingExpr{std::move(inner_expr)});
        consume(TokenType::RIGHT_PAREN, "Expected closing parenthesis symbol ')'");
    }
    // other prefix of ! and -
    else if (match({TokenType::NOT, TokenType::MINUS})) {
        const auto tok_type = previous_token().type;
        std::unique_ptr<AST::Expr> inner_expr = parse_expr_bp(get_prefix_bp(tok_type));
        lhs = std::make_unique<AST::Expr>(AST::UnaryExpr{tok_type, std::move(inner_expr)});
    }
    // atomic expression - only int, string, bool, nil literal for now
    else if (match({TokenType::NUMBER, TokenType::TRUE, TokenType::FALSE, TokenType::STRING, TokenType::NIL})) {
        const auto tok = previous_token();
        lhs = std::make_unique<AST::Expr>(AST::BasicLiteralExpr(tok));
    }
    else if (match(TokenType::IDENTIFIER)) {
        lhs = std::make_unique<AST::Expr>(AST::VariableExpr{previous_token().lexeme});
    }
    else if (match(TokenType::LEFT_SQUARE_BRACKET)) {
        std::vector<std::unique_ptr<AST::Expr>> exprs;
        exprs.emplace_back(parse_expr_bp(0));
        if (match(TokenType::DOUBLE_DOT)) {
            exprs.emplace_back(parse_expr_bp(0));
            lhs = std::make_unique<AST::Expr>(AST::RangeLiteralExpr{std::move(exprs[0]), std::move(exprs[1])});
            consume(TokenType::RIGHT_SQUARE_BRACKET, "Expected closing parenthesis ']' for Range Array literal.");
        }
        else {
            while (true) {
                if (!match(TokenType::COMMA)) break;
                exprs.emplace_back(parse_expr_bp(0));
            }
            consume(TokenType::RIGHT_SQUARE_BRACKET, "Expected closing parenthesis ']' for Array literal.");
            lhs = std::make_unique<AST::Expr>(AST::ArrayLiteralExpr{std::move(exprs)});
        }
    }
    else throw ParseException("Unexpected token in expression.");

    while (true) {
        // break on semicolon, closing paren, closing array, comma operator, double dot operator
        if (is_at_semi() || check(TokenType::RIGHT_PAREN) || check(TokenType::RIGHT_SQUARE_BRACKET)
            || check(TokenType::COMMA) || check(TokenType::DOUBLE_DOT))
            break;

        // handle post-fix first
        // arrays
        if (match(TokenType::LEFT_SQUARE_BRACKET)) {
            auto inner_expr = parse_expr_bp(0);
            consume(TokenType::RIGHT_SQUARE_BRACKET, "Expected closing symbol ']' for array.");
            lhs = std::make_unique<AST::Expr>(AST::ArrayIndexExpr{std::move(lhs), std::move(inner_expr)});
            continue;
        }
        // functions
        if (match(TokenType::LEFT_PAREN)) {
            std::vector<std::unique_ptr<AST::Expr>> exprs;
            if (!check(TokenType::RIGHT_PAREN)) while (true) {
                auto inner_expr = parse_expr_bp(0);
                exprs.push_back(std::move(inner_expr));
                if (match(TokenType::COMMA)) continue;
                break;
            }
            consume(TokenType::RIGHT_PAREN, "Expected closing parenthesis ')' in function call.");
            lhs = std::make_unique<AST::Expr>(AST::FunctionCallExpr{std::move(lhs), std::move(exprs)});
            continue;
        }
        const auto [lbp, rbp] = get_infix_bp(peek().type);
        if (lbp < min_bp) break;

        const auto op { peek().type }; // TODO: way in the future : start using { ... } init style everywhere
        advance();
        auto rhs { parse_expr_bp(rbp) };

        if (op == TokenType::EQUAL) {
            lhs = std::make_unique<AST::Expr>(AST::AssignmentExpr{std::move(lhs), std::move(rhs)});
            continue;
        }

        lhs = std::make_unique<AST::Expr>(AST::BinaryExpr{std::move(lhs), op, std::move(rhs)});
    }

    return lhs;
}
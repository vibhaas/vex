//
// Created by accord.
//

#include "parser/Parser.hpp"

#include <iostream>
#include <vector>
#include <string>
#include "lexer/Token.hpp"
#include "ast/Ast.hpp"

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
std::vector<std::unique_ptr<AST::Stmt>> Parser::take_ast()  {
    return std::move(ast);
}
std::string Parser::print_ast() const {
    std::string value;
    for (const auto &stmt : ast) {
        value += AST::print_stmt_dump(*stmt);
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
            if (std::unique_ptr<AST::Stmt> res{parse_statement()}; res != nullptr) ast.emplace_back(std::move(res));
        }
        catch (const ParseException& e) {
            errors.emplace_back(peek(), e.what());
            synchronize();
        }
        catch (const std::exception& e) {
            errors.emplace_back(peek(), std::string("Critical error! Stopping Parsing. Details : ") + e.what());
            return;
        }
    }
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

/*
 *
 * Statements
 * beyond this point
 *
 */

std::unique_ptr<AST::Stmt> Parser::parse_statement() {
    std::unique_ptr<AST::Stmt> result;
    bool no_semi = false;
    if (match(TokenType::LEFT_CURLY_BRACKET)) { no_semi = true; result = parse_block_stmt(); }
    else if (match(TokenType::FUN)) { no_semi = true; result = parse_function_stmt();}
    else if (match({TokenType::PRINT, TokenType::PRINTLN, TokenType::READ})) result = parse_io_stmt();
    else if (match(TokenType::IF)) { no_semi = true; result = parse_if_else_stmt(); }
    else if (match(TokenType::FOR)) { no_semi = true; result = parse_for_stmt(); }
    else if (match(TokenType::WHILE)) { no_semi = true; result = parse_while_stmt(); }
    else if (match(TokenType::RETURN)) result = parse_return_stmt();
    else if (match(TokenType::BREAK)) result = parse_break_stmt();
    else if (match(TokenType::CONTINUE)) result = parse_continue_stmt();
    else if (match(TokenType::EXIT)) result = parse_exit_stmt();
    else if (check(TokenType::INT32) || check(TokenType::BOOL)) result = parse_variable_decl_stmt();
    else if (match(TokenType::SEMICOLON)) return nullptr;
    else result = parse_expression_stmt();

    if (!no_semi) consume(TokenType::SEMICOLON, "Expected closing ';' for statement");
    return result;
}

std::unique_ptr<AST::Stmt> Parser::parse_block_stmt() {
    std::vector<std::unique_ptr<AST::Stmt>> stmts;
    while (!check(TokenType::RIGHT_CURLY_BRACKET) && !is_at_end()) {
        if (std::unique_ptr<AST::Stmt> res{parse_statement()}; res != nullptr) stmts.emplace_back(std::move(res));
    }
    consume(TokenType::RIGHT_CURLY_BRACKET, "Expected '}' after block");
    return std::make_unique<AST::Stmt>(AST::BlockStmt{std::move(stmts)});
}

// helper
AST::VarTypeSpec Parser::parse_var_type() {
    AST::VarTypeSpec result;
    if (check(TokenType::INT32) || check(TokenType::BOOL)) {
        if (check(TokenType::INT32)) result.type = AST::VarTypeOpts::INT32;
        else result.type = AST::VarTypeOpts::BOOL;
        advance();
        result.is_array = check(TokenType::LEFT_SQUARE_BRACKET);
        if (result.is_array) {
            advance();
            if (!check(TokenType::RIGHT_SQUARE_BRACKET)) {
                result.size = stoi(consume(TokenType::NUMBER, "Expected a numeric size for array.").lexeme);
            }
            consume(TokenType::RIGHT_SQUARE_BRACKET, "Expected ']' for array declaration.");
        }
    }
    else throw ParseException("Expected a valid type of variable.");
    return result;
}

std::unique_ptr<AST::Stmt> Parser::parse_function_stmt() {
    // needs a name, call types, return type, body
    const std::string name = consume(TokenType::IDENTIFIER, "Expected function identifier.").lexeme;
    consume(TokenType::LEFT_PAREN, "Function should open with '('.");
    std::vector<AST::Param> params;
    if (!check(TokenType::RIGHT_PAREN)) while (true) {
        auto var_type = parse_var_type();
        params.emplace_back(var_type, consume(TokenType::IDENTIFIER, "Expected variable name.").lexeme);
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RIGHT_PAREN, "Expected ')' for function.");
    consume(TokenType::ARROW, "Expected '->' for function return type.");
    std::optional<AST::VarTypeSpec> ret_type;
    if (!match(TokenType::NIL)) { // Special case ! NIL return ! Do nothing -> optional return type
        ret_type = parse_var_type();
    }
    auto body = parse_statement();
    return std::make_unique<AST::Stmt>(AST::FuncDeclStmt{name, ret_type, std::move(params), std::move(body)});
}

std::unique_ptr<AST::Stmt> Parser::parse_variable_decl_stmt() { // very hacky may the programming gods please forgive my sins ;-;
    const auto typ = parse_var_type();
    std::vector<std::unique_ptr<AST::Stmt>> res;
    while (true) {
        const std::string name = consume(TokenType::IDENTIFIER, "Expected variable name.").lexeme;
        std::unique_ptr<AST::Expr> init = nullptr;
        if (match(TokenType::EQUAL)) {
            init = parse_expr_bp(0);
        }
        res.emplace_back(std::make_unique<AST::Stmt>(AST::VarDeclStmt{typ, name, std::move(init)}));
        if (!match(TokenType::COMMA)) break;
    }
    return std::make_unique<AST::Stmt>(AST::BlockStmt{std::move(res), true});
}

std::unique_ptr<AST::Stmt> Parser::parse_if_else_stmt() {
    auto expr = parse_expr_bp(0);
    consume(TokenType::COLON, "Expected ':' after if-condition.");
    auto then_stmt = parse_statement();
    std::unique_ptr<AST::Stmt> else_stmt = nullptr;
    if (match(TokenType::ELIF)) {
        else_stmt = parse_if_else_stmt();
    }
    else if (match(TokenType::ELSE)) {
        consume(TokenType::COLON, "Expected ':' after else statement.");
        else_stmt = parse_statement();
    }
    return std::make_unique<AST::Stmt>(AST::IfElseStmt{std::move(expr), std::move(then_stmt), std::move(else_stmt)});
}

std::unique_ptr<AST::Stmt> Parser::parse_for_stmt() {
    const std::string iter_name = consume(TokenType::IDENTIFIER, "Expect a iterator variable for 'for'.").lexeme;
    consume(TokenType::IN, "'For' requires an accompanying 'in'.");
    auto expr = parse_expr_bp(0);
    consume(TokenType::COLON, "Expected ':' after 'for'.");
    auto body = parse_statement();
    return std::make_unique<AST::Stmt>(AST::ForLoopStmt{iter_name, std::move(expr), std::move(body)});
}

std::unique_ptr<AST::Stmt> Parser::parse_while_stmt() {
    auto expr = parse_expr_bp(0);
    consume(TokenType::COLON, "Expected ':' after 'while'.");
    auto body = parse_statement();
    return std::make_unique<AST::Stmt>(AST::WhileLoopStmt{std::move(expr), std::move(body)});
}

std::unique_ptr<AST::Stmt> Parser::parse_io_stmt() {
    if (previous_token().type == TokenType::PRINT) {
        consume(TokenType::LEFT_PAREN, "Expected '(' after print.");
        std::vector<std::unique_ptr<AST::Expr>> exprs;
        if (!check(TokenType::RIGHT_PAREN)) while (true) {
            exprs.emplace_back(parse_expr_bp(0));
            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' closing print.");
        return std::make_unique<AST::Stmt>(AST::IOPrintStmt{std::move(exprs)});
    }
    if (previous_token().type == TokenType::PRINTLN) {
        consume(TokenType::LEFT_PAREN, "Expected '(' after println.");
        std::vector<std::unique_ptr<AST::Expr>> exprs;
        if (!check(TokenType::RIGHT_PAREN)) while (true) {
            exprs.emplace_back(parse_expr_bp(0));
            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RIGHT_PAREN, "Expected ')' closing println.");
        return std::make_unique<AST::Stmt>(AST::IOPrintlnStmt{std::move(exprs)});
    }
    // READ
    consume(TokenType::LEFT_PAREN, "Expected '(' after read.");
    std::vector<std::string> names;
    if (!check(TokenType::RIGHT_PAREN)) while (true) {
        names.emplace_back(consume(TokenType::IDENTIFIER, "Expected an identifier for read.").lexeme);
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RIGHT_PAREN, "Expected ')' closing read.");
    return std::make_unique<AST::Stmt>(AST::IOReadStmt{std::move(names)});
}

std::unique_ptr<AST::Stmt> Parser::parse_expression_stmt() {
    std::unique_ptr<AST::Expr> expr{ parse_expr_bp(0) };
    return std::make_unique<AST::Stmt>(AST::ExprStmt{std::move(expr)});
}

std::unique_ptr<AST::Stmt> Parser::parse_return_stmt() {
    std::unique_ptr<AST::Expr> expr;
    if (!check(TokenType::SEMICOLON)) expr = parse_expr_bp(0);
    return std::make_unique<AST::Stmt>(AST::ReturnStmt{std::move(expr)});
}

std::unique_ptr<AST::Stmt> Parser::parse_break_stmt() {
    return std::make_unique<AST::Stmt>(AST::BreakStmt{});
}

std::unique_ptr<AST::Stmt> Parser::parse_continue_stmt() {
    return std::make_unique<AST::Stmt>(AST::ContinueStmt{});
}
std::unique_ptr<AST::Stmt> Parser::parse_exit_stmt() {
    consume(TokenType::LEFT_PAREN, "Expected '(' after exit.");
    std::unique_ptr<AST::Expr> expr{ parse_expr_bp(0) };
    consume(TokenType::RIGHT_PAREN, "Expected ')' after exit expression.");
    return std::make_unique<AST::Stmt>(AST::ExitStmt{std::move(expr)});
}
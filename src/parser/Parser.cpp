//
// Created by accord.
//

#include "parser/Parser.hpp"

#include <vector>
#include <string>
#include "lexer/Token.hpp"
#include "parser/Ast.hpp"

Parser::Parser(std::vector<Token> p_tokens) : tokens(std::move(p_tokens)) {};

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

// Gonna switch up and use Pratt Parsing (https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html)
// Because honestly, f*** the idea of sitting down and
// writing expression, subexpression, term, factor, atom, blah blah grammar rules
// when I can be sane
void Parser::parse() {
    // cool time to do stuff now
    // we are only expecting an expression right now so..
}

AST::Expr Parser::parse_expression() {

}
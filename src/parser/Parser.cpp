//
// Created by accord.
//

#include "parser/Parser.hpp"

#include <vector>
#include <string>
#include "lexer/Token.hpp"
#include "parser/Ast.hpp"

void Parser::parse() {

}
bool Parser::has_errors() {
    return true;
}
std::string Parser::print_errors() {
    return "Parser is not implemented, lol!";
}
std::vector<AST::Stmt> Parser::get_ast() {
    return {};
}
std::string Parser::print_ast() {
    return "";
}
//
// Created by accord.
//

#ifndef VEXC_AST_H
#define VEXC_AST_H
#include <memory>
#include <string>
#include <variant>
#include <lexer/TokenType.hpp>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace AST {
    struct Expr;

    struct LiteralExpr {
        std::string value;
    };

    struct GroupingExpr {
        std::unique_ptr<Expr> expr;
    };

    struct UnaryExpr {
        TokenType op;
        std::unique_ptr<Expr> expr;
    };

    struct BinaryExpr {
        std::unique_ptr<Expr> left;
        TokenType op;
        std::unique_ptr<Expr> right;
    };

    using ExprNode = std::variant<LiteralExpr, GroupingExpr, UnaryExpr, BinaryExpr>;

    struct Expr {
        ExprNode node;
    };

    inline std::string print_Expr(const Expr &expr) {
        return std::visit(overloaded {
            [](const LiteralExpr& lit) -> std::string {
                return "<literal: " + lit.value + ">";
            },
            [](const GroupingExpr& group) -> std::string {
                return "(group " + print_Expr(*group.expr) + ")";
            },
            [](const UnaryExpr& unary) -> std::string {
                return "([" + token_type_to_string(unary.op) + "] " + print_Expr(*unary.expr) + ")";
            },
            [](const BinaryExpr& bin) -> std::string {
                return "([" + token_type_to_string(bin.op) + "] " + print_Expr(*bin.left) + " " + print_Expr(*bin.right) + ")";
            }
        }, expr.node);
    };

    struct Stmt {
        std::unique_ptr<Expr> expr;
    };

    inline std::string print_Stmt(const Stmt &stmt) {
        return print_Expr(*stmt.expr);
    }
}

#endif //VEXC_AST_H

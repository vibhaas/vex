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
    //using ExprPtr = std::unique_ptr<Expr>;

    struct BasicLiteralExpr { // i32, bool, string
        Token value;
    };

    struct ArrayLiteralExpr { // array literal
        std::vector<std::unique_ptr<Expr>> expr_list;
    };

    struct RangeLiteralExpr { // range based arrays
        std::unique_ptr<Expr> start, end;
    };

    struct VariableExpr {
        std::string name;
    };

    struct ArrayIndexExpr { // array access my_arr[...]
        std::unique_ptr<Expr> arr_expr, index_expr;
    };

    struct FunctionCallExpr {
        std::unique_ptr<Expr> func_expr;
        std::vector<std::unique_ptr<Expr>> call_expr_list;
    };

    struct GroupingExpr { // ( ... )
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

    struct AssignmentExpr {
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
    };

    using ExprNode = std::variant<BasicLiteralExpr, ArrayLiteralExpr, RangeLiteralExpr,
        VariableExpr, ArrayIndexExpr, FunctionCallExpr, GroupingExpr, UnaryExpr, BinaryExpr, AssignmentExpr>;

    struct Expr {
        ExprNode node;
    };

    inline std::string print_expr_dump(const Expr &expr) {
        return std::visit(overloaded {
            [](const BasicLiteralExpr& exp) -> std::string {
                return "<BasicLiteralExpr: " + exp.value.to_string() + ">";
            },
            [](const ArrayLiteralExpr& exp) -> std::string {
                std::string resp = "<ArrayLiteralExpr: ";
                for (const auto& el : exp.expr_list) {
                    resp += print_expr_dump(*el) + ", ";
                }
                resp += ">";
                return resp;
            },
            [](const RangeLiteralExpr& exp) -> std::string {
                return "<RangeLiteralExpr: " + print_expr_dump(*exp.start) + ", " + print_expr_dump(*exp.end) + ">";
            },
            [](const VariableExpr& exp) -> std::string {
                return "<<VariableExpr: " + exp.name + ">>";
            },
            [](const ArrayIndexExpr& exp) -> std::string {
                return "<<ArrayIndexExpr: " + print_expr_dump(*exp.arr_expr) + "[" + print_expr_dump(*exp.index_expr) + "] >>";
            },
            [](const FunctionCallExpr& exp) -> std::string {
                std::string resp = "<<FunctionCallExpr: " + print_expr_dump(*exp.func_expr) + "(";
                for (const auto& el : exp.call_expr_list) {
                    resp += print_expr_dump(*el) + ", ";
                }
                resp += ") >>";
                return resp;
            },
            [](const GroupingExpr& exp) -> std::string {
                return "(GroupingExpr: " + print_expr_dump(*exp.expr) + ")";
            },
            [](const UnaryExpr& exp) -> std::string {
                return "(UnaryExpr[" + token_type_to_string(exp.op) + "]: " + print_expr_dump(*exp.expr) + ")";
            },
            [](const BinaryExpr& exp) -> std::string {
                return "(BinaryExpr[" + token_type_to_string(exp.op) + "]: " + print_expr_dump(*exp.left) + " " + print_expr_dump(*exp.right) + ")";
            },
            [](const AssignmentExpr& exp) -> std::string {
                return "(AssignmentExpr: " + print_expr_dump(*exp.left) + " = " + print_expr_dump(*exp.right) + ")";
            }
        }, expr.node);
    }

    struct Stmt {
        std::unique_ptr<Expr> expr;
    };

    inline std::string print_stmt(const Stmt &stmt) {
        return print_expr_dump(*stmt.expr);
    }

    inline std::string pretty_print_expr(const Expr &expr) {
        return "TODO"; // TODO : implement this
    }

    inline std::string pretty_print_stmt(const Stmt &stmt) {
        return pretty_print_expr(*stmt.expr);
    }
}

#endif //VEXC_AST_H

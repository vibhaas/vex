//
// Created by accord.
//

#ifndef VEXC_AST_H
#define VEXC_AST_H
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <lexer/TokenType.hpp>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace AST {
    struct Expr;
    //using ExprPtr = std::unique_ptr<Expr>;

    struct BasicLiteralExpr { // i32, bool, string, nil
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

    struct Stmt;

    struct ExprStmt {
        std::unique_ptr<Expr> expr;
    };

    struct BlockStmt {
        std::vector<std::unique_ptr<Stmt>> stmt_list;
    };

    /*** Var Types - helper structs ***/

    enum class VarTypeOpts { // int32, bool (true / false)
        INT32, BOOL
    };

    struct VarTypeSpec {
        VarTypeOpts type;
        bool is_array;
        std::optional<int> size;
    };

    inline std::string print_var_type_spec_dump(const VarTypeSpec& type) { // for future dumping
        std::string resp = (type.type == VarTypeOpts::INT32) ? "i32" : "bool";
        if (type.is_array) {
            resp += "[";
            if (type.size) resp += std::to_string(*type.size);
            resp += "]";
        }
        return resp;
    }

    /*** End Var Types ***/

    struct VarDeclStmt {
        VarTypeSpec type;
        std::string name;
        std::unique_ptr<Expr> init;
    };

    struct IOPrintStmt {
        std::vector<std::unique_ptr<Expr>> expr_list;
    };

    struct IOPrintlnStmt {
        std::vector<std::unique_ptr<Expr>> expr_list;
    };

    struct IOReadStmt {
        std::vector<std::string> names;
    };

    struct IfElseStmt {
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Stmt> then_branch, else_branch;
    };

    struct Param { // helper struct
        VarTypeSpec type;
        std::string name;
    };

    struct FuncDeclStmt {
        std::string name;
        std::optional<VarTypeSpec> return_type;
        std::vector<Param> param_list;
        std::unique_ptr<Stmt> body;
    };

    struct ReturnStmt {
        std::unique_ptr<Expr> expr; // note -> nil returns will be a nullptr here
    };

    struct ForLoopStmt {
        std::string iter_name;
        std::unique_ptr<Expr> iterable;
        std::unique_ptr<Stmt> body;
    };

    struct WhileLoopStmt {
        std::unique_ptr<Expr> cond;
        std::unique_ptr<Stmt> body;
    };

    struct BreakStmt {};
    struct ContinueStmt {};
    struct ExitStmt {
        std::unique_ptr<Expr> expr;
    };

    using StmtNode = std::variant<ExprStmt, BlockStmt, VarDeclStmt, IOPrintStmt, IOPrintlnStmt, IOReadStmt, IfElseStmt, FuncDeclStmt,
    ReturnStmt, ForLoopStmt, WhileLoopStmt, BreakStmt, ContinueStmt, ExitStmt>;

    struct Stmt {
        StmtNode node;
    };

    // dump of statements
    inline std::string print_stmt_dump(const Stmt& stmt) {
        auto join_exprs = [](const auto& vec) { // helper
            std::string resp;
            for (size_t i = 0; i < vec.size(); i++) {
                if (i) resp += ", ";
                resp += print_expr_dump(*vec[i]);
            }
            return resp;
        };

        auto join_names = [](const auto& vec) { // helper
            std::string resp;
            for (size_t i = 0; i < vec.size(); i++) {
                if (i) resp += ", ";
                resp += vec[i];
            }
            return resp;
        };

        return std::visit(overloaded{
            [&](const ExprStmt& st) -> std::string {
                return "<ExprStmt: " + print_expr_dump(*st.expr) + ">";
            },
            [&](const BlockStmt& st) -> std::string {
                std::string resp = "<BlockStmt: ";
                for (const auto& el : st.stmt_list) resp += print_stmt_dump(*el) + "; ";
                resp += ">";
                return resp;
            },
            [&](const VarDeclStmt& st) -> std::string {
                std::string resp = "<VarDeclStmt: " + print_var_type_spec_dump(st.type) + " " + st.name;
                if (st.init) resp += " = " + print_expr_dump(*st.init);
                resp += ">";
                return resp;
            },
            [&](const IOPrintStmt& st) -> std::string {
                return "<IOPrintStmt: PRINT(" + join_exprs(st.expr_list) + ")>";
            },
            [&](const IOPrintlnStmt& st) -> std::string {
                return "<IOPrintlnStmt: PRINTLN(" + join_exprs(st.expr_list) + ")>";
            },
            [&](const IOReadStmt& st) -> std::string {
                return "<IOReadStmt: READ(" + join_names(st.names) + ")>";
            },
            [&](const IfElseStmt& st) -> std::string {
                std::string resp = "<IfElseStmt: IF " + print_expr_dump(*st.condition)
                                 + " THEN " + print_stmt_dump(*st.then_branch);
                if (st.else_branch) resp += " ELSE " + print_stmt_dump(*st.else_branch);
                resp += ">";
                return resp;
            },
            [&](const FuncDeclStmt& st) -> std::string {
                std::string resp = "<FuncDeclStmt: FUNCTION " + st.name + "(";
                for (size_t i = 0; i < st.param_list.size(); i++) {
                    if (i) resp += ", ";
                    resp += print_var_type_spec_dump(st.param_list[i].type) + " " + st.param_list[i].name;
                }
                resp += ")";
                if (st.return_type) resp += " -> " + print_var_type_spec_dump(*st.return_type);
                resp += " " + print_stmt_dump(*st.body) + ">";
                return resp;
            },
            [&](const ReturnStmt& st) -> std::string {
                return st.expr ? "<ReturnStmt: RETURN " + print_expr_dump(*st.expr) + ">"
                               : "<ReturnStmt: RETURN>";
            },
            [&](const ForLoopStmt& st) -> std::string {
                return "<ForLoopStmt: FOR " + st.iter_name + " IN " + print_expr_dump(*st.iterable)
                     + " DO " + print_stmt_dump(*st.body) + ">";
            },
            [&](const WhileLoopStmt& st) -> std::string {
                return "<WhileLoopStmt: WHILE " + print_expr_dump(*st.cond)
                     + " DO " + print_stmt_dump(*st.body) + ">";
            },
            [&](const BreakStmt&) -> std::string {
                return "<BreakStmt>";
            },
            [&](const ContinueStmt&) -> std::string {
                return "<ContinueStmt>";
            },
            [&](const ExitStmt& st) -> std::string {
                return "<ExitStmt: EXIT(" + print_expr_dump(*st.expr) + ")>";
            }
        }, stmt.node);
    }

    // inline std::string pretty_print_expr(const Expr &expr) {
    //     return "TODO"; // TODO : implement this
    // }
    //
    // inline std::string pretty_print_stmt(const Stmt &stmt) {
    //     return pretty_print_expr(*stmt.expr);
    // }
}

#endif //VEXC_AST_H

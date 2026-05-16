//
// Created by accord.
//

#ifndef VEXC_AST_H
#define VEXC_AST_H
#include <memory>
#include <string>

namespace AST {
    struct Expr;

    struct Literal {
        std::string value;
    };

    struct GroupingExpr {
        std::unique_ptr<Expr> expr;
    };

    struct UnaryExpr {
        std::string op;
        std::unique_ptr<Expr> expr;
    };

    struct BinaryExpr {
        std::unique_ptr<Expr> left;
        std::string op;
        std::unique_ptr<Expr> right;
    };

    std::string print_AST(const Expr &expr);
}

#endif //VEXC_AST_H

//
// Created by accord.
//

#ifndef VEXC_VEXIRGENERATOR_HPP
#define VEXC_VEXIRGENERATOR_HPP

#include <memory>
#include <string>
#include <vector>
#include "ast/Ast.hpp"
#include "vex-ir/VexIR.hpp"

class VexIRGenerator {
private:
    std::vector<std::unique_ptr<AST::Stmt>> ast;
    VexIR::Module generated_ir;
    VexIR::Module optimized_ir;
    bool is_error = false;
public:
    explicit VexIRGenerator(std::vector<std::unique_ptr<AST::Stmt>> p_ast);
    void build();
    [[nodiscard]] bool error() const { return is_error; }
    [[nodiscard]] std::string print_ir() const;
    [[nodiscard]] std::string print_raw_ir() const;
};

#endif //VEXC_VEXIRGENERATOR_HPP

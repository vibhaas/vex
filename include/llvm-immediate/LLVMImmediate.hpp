//
// Created by accord.
//

#ifndef VEXC_LLVMIMMEDIATE_HPP
#define VEXC_LLVMIMMEDIATE_HPP

#include <vector>
#include <memory>
#include "ast/Ast.hpp"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>

class LLVMImmException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class LLVMImmediate {
private:
    std::vector<std::unique_ptr<AST::Stmt>> ast;
    bool is_error = false;

    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::vector<std::unordered_map<std::string, llvm::Value*>> symbols;

    std::vector<llvm::BasicBlock*> break_stack;
    std::vector<llvm::BasicBlock*> continue_stack;

    llvm::Value* emit_expr(const AST::Expr& expr);
    void emit_stmt(const AST::Stmt& stmt);

    [[nodiscard]] llvm::Type* get_llvm_type(const AST::VarTypeSpec& type_spec) const;
    [[nodiscard]] llvm::Value* get_symbol_ptr(const std::string& name) const;

public:
    explicit LLVMImmediate(std::vector<std::unique_ptr<AST::Stmt>> p_ast);
    void build();
    [[nodiscard]] constexpr bool error() const { return is_error; }
    [[nodiscard]] std::string print_llvm() const;
    void execute();
};

#endif //VEXC_LLVMIMMEDIATE_HPP

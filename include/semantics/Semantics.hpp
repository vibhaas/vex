//
// Created by accord.
//

#ifndef VEXC_SEMANTICS_H
#define VEXC_SEMANTICS_H
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "ast/Ast.hpp"

class SemanticsException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Symbol {
    std::string name;
    AST::VarTypeSpec type;
};

struct FunctionInfo {
    std::string name;
    std::optional<AST::VarTypeSpec> return_type;
    std::vector<AST::VarTypeSpec> parameter_types;
};

class SemanticAnalyzer {
private:
    std::vector<std::unique_ptr<AST::Stmt>> ast;
    std::vector<std::unordered_map<std::string,Symbol>> symbol_table;
    std::unordered_map<std::string,FunctionInfo> function_table;
    bool is_error = false;

    std::optional<AST::VarTypeSpec> current_return_type;
    int function_depth = 0;
    int loop_depth = 0;

    void register_function(const std::string& name, const std::optional<AST::VarTypeSpec> &type,
        const std::vector<AST::VarTypeSpec> &param_types);
    void register_symbol(const std::string& name, const AST::VarTypeSpec &type);
    bool does_exist_function(const std::string& name) const;
    bool does_exist_symbol(const std::string& name, bool current_scope) const;
    void bump_scope();
    void pop_scope();
    FunctionInfo get_function_info(const std::string& name) const;
    const Symbol& get_symbol(const std::string& name) const;
    AST::VarTypeSpec analyze_expr(AST::Expr& expr);
    void analyze_stmt(AST::Stmt& stmt);
    static bool types_compatible(const AST::VarTypeSpec& a, const AST::VarTypeSpec& b) ;

public:
    explicit SemanticAnalyzer(std::vector<std::unique_ptr<AST::Stmt>> p_ast);
    void analyze();
    constexpr bool error() const { return is_error; }
    [[nodiscard]] std::string print_ast() const;
};

#endif //VEXC_SEMANTICS_H

//
// Created by accord.
//

#include "semantics/Semantics.hpp"

#include <iostream>

SemanticAnalyzer::SemanticAnalyzer(std::vector<std::unique_ptr<AST::Stmt>> p_ast) : ast(std::move(p_ast)) {
    bump_scope();
}

std::vector<std::unique_ptr<AST::Stmt>> SemanticAnalyzer::take_ast()  {
    return std::move(ast);
}

void SemanticAnalyzer::analyze() {
    // first, function registrations
    // functions have to be top-level scope
    try {
        for (const auto &stmt : ast) {
            if (const auto* func = std::get_if<AST::FuncDeclStmt>(&stmt->node)) {
                std::vector<AST::VarTypeSpec> param_types;
                for (const auto&[type, name] : func->param_list) {
                    param_types.emplace_back(type);
                }
                register_function(func->name, func->return_type, param_types);
            }
        }

        // now actual analysis

        for (const auto &stmt : ast) {
            analyze_stmt(*stmt);
        }
    }
    catch (const SemanticsException &e) {
        std::cerr << "Semantic Error : " << e.what() << std::endl;
        is_error = true;
    }
    catch (const std::exception &e) {
        std::cerr << "CRITICAL Semantic Error : " << e.what() << std::endl;
        is_error = true;
    }
}

// statement analysis
void SemanticAnalyzer::analyze_stmt(AST::Stmt& stmt) {
    std::visit(overloaded{
        // expressions -> just call the expr analysis func
        [&](const AST::ExprStmt& st)->void {
            analyze_expr(*st.expr);
        },
        // block statements
        [&](const AST::BlockStmt& st)->void {
            if (!st.special_dont_change_scope) bump_scope();
            try {
                for (const auto& child : st.stmt_list) analyze_stmt(*child);
            } catch (...) {
                if (!st.special_dont_change_scope) pop_scope();
                throw;
            }
            if (!st.special_dont_change_scope) pop_scope();
        },
        // variable declare statements
        [&](const AST::VarDeclStmt& st)->void {
            if (st.init) {
                const auto init_type = analyze_expr(*st.init);
                if (!types_compatible(st.type, init_type)) {
                    throw SemanticsException("Type mismatch while initializing variable " + st.name + ".");
                }
            }
            register_symbol(st.name, st.type);
        },
        // if else
        [&](const AST::IfElseStmt& st) -> void {
            const auto cond_type = analyze_expr(*st.condition);
            if (cond_type.type != AST::VarTypeOpts::BOOL || cond_type.is_array) {
                throw SemanticsException("If condition must be scalar BOOL.");
            }
            analyze_stmt(*st.then_branch);
            if (st.else_branch) analyze_stmt(*st.else_branch);
        },
        // declaration of func -> forcing it to be top-level
        [&](const AST::FuncDeclStmt& st) -> void {
            if (symbol_table.size() != 1) {
                throw SemanticsException("Function " + st.name + " must be declared at top level scope only.");
            }

            const auto info = get_function_info(st.name);
            if (st.param_list.size() != info.parameter_types.size()) {
                throw SemanticsException("Function " + st.name + " parameter count mismatch.");
            }

            bump_scope();
            const auto prev_return_type = current_return_type;
            current_return_type = info.return_type;
            ++function_depth;

            try {
                for (size_t i = 0; i < st.param_list.size(); i++) {
                    const auto& param = st.param_list[i];
                    if (!types_compatible(param.type, info.parameter_types[i])) {
                        throw SemanticsException("Function " + st.name + " parameter type mismatch at parameter " + param.name + ".");
                    }
                    register_symbol(param.name, param.type);
                }
                analyze_stmt(*st.body);
            } catch (...) {
                --function_depth;
                current_return_type = prev_return_type;
                pop_scope();
                throw;
            }

            --function_depth;
            current_return_type = prev_return_type;
            pop_scope();
        },
        // ensure return types match
        [&](const AST::ReturnStmt& st) -> void {
            if (function_depth == 0) {
                throw SemanticsException("Return statement found outside of a function.");
            }

            if (!current_return_type.has_value()) {
                if (st.expr) {
                    throw SemanticsException("Function returns nil, so return statement must not have a value.");
                }
                return;
            }

            if (!st.expr) {
                throw SemanticsException("Function must return " + print_var_type_spec_dump(*current_return_type) + ".");
            }

            const auto ret_type = analyze_expr(*st.expr);
            if (!types_compatible(*current_return_type, ret_type)) {
                throw SemanticsException("Return type mismatch. Expected " + print_var_type_spec_dump(*current_return_type) + ", got " + print_var_type_spec_dump(ret_type) + ".");
            }
        },
        // for loops
        [&](const AST::ForLoopStmt& st) -> void {
            const auto iterable_type = analyze_expr(*st.iterable);
            if (!iterable_type.is_array) {
                throw SemanticsException("For loop requires an array or range expression.");
            }
            if (iterable_type.type == AST::VarTypeOpts::STRING_LITERAL) {
                throw SemanticsException("For loop cannot iterate over string literals.");
            }

            AST::VarTypeSpec iter_type = iterable_type;
            iter_type.is_array = false;
            iter_type.size = std::nullopt;

            bump_scope();
            ++loop_depth;
            try {
                register_symbol(st.iter_name, iter_type);
                analyze_stmt(*st.body);
            } catch (...) {
                --loop_depth;
                pop_scope();
                throw;
            }
            --loop_depth;
            pop_scope();
        },
        // while loops
        [&](const AST::WhileLoopStmt& st) -> void {
            const auto cond_type = analyze_expr(*st.cond);
            if (cond_type.type != AST::VarTypeOpts::BOOL || cond_type.is_array) {
                throw SemanticsException("While condition must be scalar BOOL.");
            }

            ++loop_depth;
            try {
                analyze_stmt(*st.body);
            } catch (...) {
                --loop_depth;
                throw;
            }
            --loop_depth;
        },
        // print
        [&](const AST::IOPrintStmt& st) -> void {
            for (const auto& e : st.expr_list) {
                const auto typ = analyze_expr(*e);
                if (typ.is_array) throw SemanticsException("print() does not accept arrays.");
                if (typ.type != AST::VarTypeOpts::INT32 &&
                    typ.type != AST::VarTypeOpts::BOOL &&
                    typ.type != AST::VarTypeOpts::STRING_LITERAL) {
                    throw SemanticsException("print() accepts only scalar INT32, BOOL, and string literals.");
                }
            }
        }, // println
        [&](const AST::IOPrintlnStmt& st) -> void {
            for (const auto& e : st.expr_list) {
                const auto typ = analyze_expr(*e);
                if (typ.is_array) throw SemanticsException("println() does not accept arrays.");
                if (typ.type != AST::VarTypeOpts::INT32 &&
                    typ.type != AST::VarTypeOpts::BOOL &&
                    typ.type != AST::VarTypeOpts::STRING_LITERAL) {
                    throw SemanticsException("println() accepts only INT32, BOOL, and string literals.");
                }
            }
        },
        // read
        [&](const AST::IOReadStmt& st) -> void {
            for (const auto& name : st.names) {
                const auto& sym = get_symbol(name);
                if (sym.type.is_array) throw SemanticsException("Cannot read directly into array variable " + name + ".");
                if (sym.type.type == AST::VarTypeOpts::STRING_LITERAL) throw SemanticsException("Cannot read directly into string variable " + name + ".");
            }
        },
        // break and continue
        [&](const AST::BreakStmt&) -> void {
            if (loop_depth == 0) throw SemanticsException("break statement found outside of a loop.");
        },
        [&](const AST::ContinueStmt&) -> void {
            if (loop_depth == 0) throw SemanticsException("continue statement found outside of a loop.");
        },
        // exit
        [&](const AST::ExitStmt& st) -> void {
            const auto typ = analyze_expr(*st.expr);
            if (typ.type != AST::VarTypeOpts::INT32 || typ.is_array) {
                throw SemanticsException("exit() requires a INT32 expression.");
            }
        }
    }, stmt.node);
}

// expressions type analysis
AST::VarTypeSpec SemanticAnalyzer::analyze_expr(AST::Expr& expr) {
    return std::visit(overloaded {
        // basic literal
        [&](const AST::BasicLiteralExpr& lit) -> AST::VarTypeSpec {
            AST::VarTypeSpec typ;
            switch (lit.value.type) {
                case TokenType::NUMBER:
                    typ = { AST::VarTypeOpts::INT32, false, std::nullopt };
                    break;
                case TokenType::TRUE:
                case TokenType::FALSE:
                    typ = { AST::VarTypeOpts::BOOL, false, std::nullopt };
                    break;
                case TokenType::STRING:
                    typ = { AST::VarTypeOpts::STRING_LITERAL, false, std::nullopt};
                    break;
                default:
                    throw SemanticsException("Invalid literal token.");
            }
            expr.inferred_type = typ;
            return typ;
        },
        // unary expressions
        [&](const AST::UnaryExpr& un) -> AST::VarTypeSpec {
            const auto rhs_type = analyze_expr(*un.expr);
            AST::VarTypeSpec result;
            switch (un.op) {
                case TokenType::MINUS:
                    if (rhs_type.type != AST::VarTypeOpts::INT32 || rhs_type.is_array) {
                        throw SemanticsException("Unary '-' operator requires INT32.");
                    }
                    result = { AST::VarTypeOpts::INT32, false, std::nullopt };
                    break;
                case TokenType::NOT:
                    if (rhs_type.type != AST::VarTypeOpts::BOOL || rhs_type.is_array) {
                        throw SemanticsException("Unary '!' operator requires BOOL.");
                    }
                    result = { AST::VarTypeOpts::BOOL, false, std::nullopt };
                    break;
                default:
                    throw SemanticsException("Invalid unary operator.");
            }
            expr.inferred_type = result;
            return result;
        },
        // binary expression
        [&](const AST::BinaryExpr& bin) -> AST::VarTypeSpec {
            const auto lhs_type = analyze_expr(*bin.left);
            const auto rhs_type = analyze_expr(*bin.right);

            if (lhs_type.type == AST::VarTypeOpts::STRING_LITERAL ||
                rhs_type.type == AST::VarTypeOpts::STRING_LITERAL) {
                throw SemanticsException("String literals are only allowed in print and println.");
            }

            AST::VarTypeSpec result;

            switch (bin.op) {
                case TokenType::PLUS:
                case TokenType::MINUS:
                case TokenType::MULTIPLY:
                case TokenType::DIVIDE:
                case TokenType::MODULO:
                    if (lhs_type.type != AST::VarTypeOpts::INT32 || lhs_type.is_array ||
                        rhs_type.type != AST::VarTypeOpts::INT32 || rhs_type.is_array) {
                        throw SemanticsException("Arithmetic operators require INT32.");
                    }
                    result = { AST::VarTypeOpts::INT32, false, std::nullopt };
                    break;

                case TokenType::GREATER:
                case TokenType::GREATER_EQUAL:
                case TokenType::LESS:
                case TokenType::LESS_EQUAL:
                    if (lhs_type.type != AST::VarTypeOpts::INT32 || lhs_type.is_array ||
                        rhs_type.type != AST::VarTypeOpts::INT32 || rhs_type.is_array) {
                        throw SemanticsException("Comparison operators require INT32.");
                    }
                    result = { AST::VarTypeOpts::BOOL, false, std::nullopt };
                    break;

                case TokenType::EQUAL_EQUAL:
                case TokenType::NOT_EQUAL:
                    if (lhs_type.type != rhs_type.type ||
                        lhs_type.is_array != rhs_type.is_array ||
                        lhs_type.size != rhs_type.size) {
                        throw SemanticsException("Equality operators require matching types.");
                    }
                    result = { AST::VarTypeOpts::BOOL, false, std::nullopt };
                    break;

                case TokenType::AND:
                case TokenType::OR:
                    if (lhs_type.type != AST::VarTypeOpts::BOOL || lhs_type.is_array ||
                        rhs_type.type != AST::VarTypeOpts::BOOL || rhs_type.is_array) {
                        throw SemanticsException("Logical operators require BOOL.");
                    }
                    result = { AST::VarTypeOpts::BOOL, false, std::nullopt };
                    break;

                default:
                    throw SemanticsException("Invalid binary operator.");
            }

            expr.inferred_type = result;
            return result;
        },
        // assignment expr
        [&](const AST::AssignmentExpr& assign_expr) -> AST::VarTypeSpec {
            if (!std::holds_alternative<AST::VariableExpr>(assign_expr.left->node) &&
                !std::holds_alternative<AST::ArrayIndexExpr>(assign_expr.left->node)) {
                throw SemanticsException("Left-hand side of assignment must be assignable.");
            }

            const auto rhs_type = analyze_expr(*assign_expr.right);
            const auto lhs_type = analyze_expr(*assign_expr.left);

            if (lhs_type.type != rhs_type.type ||
                lhs_type.is_array != rhs_type.is_array ||
                lhs_type.size != rhs_type.size) {
                throw SemanticsException("Assignment requires matching types.");
            }

            expr.inferred_type = lhs_type;
            return lhs_type;
        },
        // array literal expressions
        [&](const AST::ArrayLiteralExpr& arr) -> AST::VarTypeSpec {
            if (arr.expr_list.empty()) {
                throw SemanticsException("Empty array literal is not allowed.");
            }

            const auto first_type = analyze_expr(*arr.expr_list[0]);
            if (first_type.type == AST::VarTypeOpts::STRING_LITERAL || first_type.is_array) {
                throw SemanticsException("Array literal elements must be INT32 or BOOL.");
            }

            for (size_t i = 1; i < arr.expr_list.size(); i++) {
                const auto cur_type = analyze_expr(*arr.expr_list[i]);
                if (cur_type.type != first_type.type || cur_type.is_array || cur_type.size != first_type.size) {
                    throw SemanticsException("Array literal elements must have matching types.");
                }
            }

            AST::VarTypeSpec result = { first_type.type, true, static_cast<int>(arr.expr_list.size()) };
            arr.expr_list[0]->inferred_type = first_type;
            expr.inferred_type = result;
            return result;
        },
        // range literal expressions
        [&](const AST::RangeLiteralExpr& range) -> AST::VarTypeSpec {
            const auto start_type = analyze_expr(*range.start);
            const auto end_type = analyze_expr(*range.end);

            if (start_type.type != AST::VarTypeOpts::INT32 || start_type.is_array ||
                end_type.type != AST::VarTypeOpts::INT32 || end_type.is_array) {
                throw SemanticsException("Range literal endpoints must be INT32.");
            }

            AST::VarTypeSpec result = { AST::VarTypeOpts::INT32, true, std::nullopt };
            expr.inferred_type = result;
            return result;
        },

        [&](const AST::VariableExpr& var) -> AST::VarTypeSpec {
            const auto sym = get_symbol(var.name);
            expr.inferred_type = sym.type;
            return sym.type;
        },

        [&](const AST::ArrayIndexExpr& idx) -> AST::VarTypeSpec {
            const auto arr_type = analyze_expr(*idx.arr_expr);
            const auto index_type = analyze_expr(*idx.index_expr);

            if (!arr_type.is_array) {
                throw SemanticsException("Indexing requires an array expression.");
            }
            if (index_type.type != AST::VarTypeOpts::INT32 || index_type.is_array) {
                throw SemanticsException("Array index must be INT32.");
            }

            AST::VarTypeSpec result = arr_type;
            result.is_array = false;
            result.size = std::nullopt;

            expr.inferred_type = result;
            return result;
        },

        [&](const AST::FunctionCallExpr& call) -> AST::VarTypeSpec {
            if (!std::holds_alternative<AST::VariableExpr>(call.func_expr->node)) {
                throw SemanticsException("Function call target must be a function name.");
            }

            const auto func_name = std::get<AST::VariableExpr>(call.func_expr->node).name;
            const auto info = get_function_info(func_name);

            if (info.parameter_types.size() != call.call_expr_list.size()) {
                throw SemanticsException("Function call argument count mismatch.");
            }

            for (size_t i = 0; i < call.call_expr_list.size(); i++) {
                const auto arg_type = analyze_expr(*call.call_expr_list[i]);
                const auto& param_type = info.parameter_types[i];

                if (arg_type.type != param_type.type ||
                    arg_type.is_array != param_type.is_array ||
                    arg_type.size != param_type.size) {
                    throw SemanticsException("Function call argument type mismatch.");
                }
            }

            if (!info.return_type.has_value()) {
                throw SemanticsException("Function does not return a value.");
            }

            expr.inferred_type = info.return_type.value();
            return info.return_type.value();
        },

        [&](const AST::GroupingExpr& grp) -> AST::VarTypeSpec {
            const auto inner_type = analyze_expr(*grp.expr);
            expr.inferred_type = inner_type;
            return inner_type;
        }
    }, expr.node);
}


// helper functions

void SemanticAnalyzer::register_function(const std::string& name, const std::optional<AST::VarTypeSpec> &type,
    const std::vector<AST::VarTypeSpec> &param_types) {
    if (does_exist_function(name)) throw SemanticsException("Function " + name + " already exists.");
    function_table[name] = FunctionInfo{name, type, param_types};
}

void SemanticAnalyzer::register_symbol(const std::string &name, const AST::VarTypeSpec &type) {
    if (does_exist_function(name)) throw SemanticsException("Symbol " + name + " conflicts with an existing function name.");
    if (does_exist_symbol(name, true)) throw SemanticsException("Symbol " + name + " already exists in this scope.");
    symbol_table.back()[name] = Symbol{name, type};
}

bool SemanticAnalyzer::does_exist_function(const std::string& name) const {
    return function_table.contains(name);
}

FunctionInfo SemanticAnalyzer::get_function_info(const std::string& name) const {
    if (!does_exist_function(name)) {
        throw SemanticsException("Function " + name + " not declared before use.");
    }
    return function_table.at(name);
}

bool SemanticAnalyzer::does_exist_symbol(const std::string& name, const bool current_scope = false) const {
    for (int i = static_cast<int>(symbol_table.size()) - 1; i >= 0; --i) {
        if (symbol_table.at(i).contains(name)) return true;
        if (current_scope) return false;
    }
    return false;
}

const Symbol& SemanticAnalyzer::get_symbol(const std::string& name) const {
    for (int i = static_cast<int>(symbol_table.size()) - 1; i >= 0; --i) {
        if (symbol_table.at(i).contains(name)) return symbol_table.at(i).at(name);
    }
    throw SemanticsException("Symbol " + name + " not declared before use.");
}

void SemanticAnalyzer::bump_scope() {
    symbol_table.emplace_back();
}

void SemanticAnalyzer::pop_scope() {
    symbol_table.pop_back();
}

bool SemanticAnalyzer::types_compatible(const AST::VarTypeSpec& a, const AST::VarTypeSpec& b) {
    if (a.type != b.type || a.is_array != b.is_array) return false;
    if (!a.is_array) return true;
    if (!a.size.has_value()) return true;
    if (!b.size.has_value()) return false;
    return a.size.value() == b.size.value();
}

std::string SemanticAnalyzer::print_ast() const {
    std::string value;
    for (const auto &stmt : ast) {
        value += AST::print_stmt_dump_annotated(*stmt);
        value += " END_STATEMENT; \n";
    }
    return value;
}


//
// Created by accord.
//

#include "vex-ir-gen/VexIRGenerator.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
    using BindingMap = std::unordered_map<std::string, struct Binding>;
    using ScopeFrame = std::vector<std::pair<std::string, std::optional<struct Binding>>>;
    using ScopeStack = std::vector<ScopeFrame>;

    struct Binding {
        enum class Kind { SSA_VALUE, STORAGE_OBJECT, ADDRESS_VALUE };

        Kind kind;
        AST::VarTypeSpec type;
        std::optional<VexIR::Temporary> value;
        std::optional<VexIR::Storage> storage;
        std::optional<VexIR::Storage> spill_storage;
    };

    struct LoopFrame {
        std::string cond_block;
        std::string exit_block;
        std::unordered_set<std::string> carried_names;
        std::vector<std::pair<std::string, BindingMap>> continue_states;
        std::vector<std::pair<std::string, BindingMap>> break_states;
    };

    class VexIRGenException : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    bool bindings_equal(const Binding& lhs, const Binding& rhs) {
        if (lhs.kind != rhs.kind) return false;
        if (lhs.type.type != rhs.type.type || lhs.type.is_array != rhs.type.is_array || lhs.type.size != rhs.type.size) return false;
        switch (lhs.kind) {
            case Binding::Kind::SSA_VALUE:
            case Binding::Kind::ADDRESS_VALUE:
                return lhs.value.has_value() && rhs.value.has_value() && lhs.value->name == rhs.value->name;
            case Binding::Kind::STORAGE_OBJECT:
                return lhs.storage.has_value() && rhs.storage.has_value() && lhs.storage->name == rhs.storage->name;
        }
        return false;
    }

    VexIR::BinaryOp binary_op_from_token(const TokenType type) {
        switch (type) {
            case TokenType::PLUS: return VexIR::BinaryOp::ADD;
            case TokenType::MINUS: return VexIR::BinaryOp::SUB;
            case TokenType::MULTIPLY: return VexIR::BinaryOp::MUL;
            case TokenType::DIVIDE: return VexIR::BinaryOp::DIV;
            case TokenType::MODULO: return VexIR::BinaryOp::REM;
            case TokenType::AND: return VexIR::BinaryOp::AND;
            case TokenType::OR: return VexIR::BinaryOp::OR;
            case TokenType::GREATER: return VexIR::BinaryOp::GT;
            case TokenType::GREATER_EQUAL: return VexIR::BinaryOp::GEQ;
            case TokenType::LESS: return VexIR::BinaryOp::LT;
            case TokenType::LESS_EQUAL: return VexIR::BinaryOp::LEQ;
            case TokenType::EQUAL_EQUAL: return VexIR::BinaryOp::EQ;
            case TokenType::NOT_EQUAL: return VexIR::BinaryOp::NEQ;
            default:
                throw VexIRGenException("VexIR lowering error: Unsupported binary operator " + token_type_to_string(type) + ".");
        }
    }

    VexIR::UnaryOp unary_op_from_token(const TokenType type) {
        switch (type) {
            case TokenType::MINUS: return VexIR::UnaryOp::NEG;
            case TokenType::NOT: return VexIR::UnaryOp::NOT;
            default:
                throw VexIRGenException("VexIR lowering error: Unsupported unary operator " + token_type_to_string(type) + ".");
        }
    }

    std::optional<int> try_eval_const_i32(const AST::Expr& expr) {
        return std::visit(overloaded{
            [](const AST::BasicLiteralExpr& lit) -> std::optional<int> {
                if (lit.value.type != TokenType::NUMBER) return std::nullopt;
                return std::stoi(lit.value.lexeme);
            },
            [&](const AST::GroupingExpr& grp) -> std::optional<int> {
                return try_eval_const_i32(*grp.expr);
            },
            [&](const AST::UnaryExpr& un) -> std::optional<int> {
                const auto inner = try_eval_const_i32(*un.expr);
                if (!inner) return std::nullopt;
                if (un.op == TokenType::MINUS) return -*inner;
                return std::nullopt;
            },
            [&](const AST::BinaryExpr& bin) -> std::optional<int> {
                const auto lhs = try_eval_const_i32(*bin.left);
                const auto rhs = try_eval_const_i32(*bin.right);
                if (!lhs || !rhs) return std::nullopt;
                switch (bin.op) {
                    case TokenType::PLUS: return *lhs + *rhs;
                    case TokenType::MINUS: return *lhs - *rhs;
                    case TokenType::MULTIPLY: return *lhs * *rhs;
                    case TokenType::DIVIDE:
                        if (*rhs == 0) return std::nullopt;
                        return *lhs / *rhs;
                    case TokenType::MODULO:
                        if (*rhs == 0) return std::nullopt;
                        return *lhs % *rhs;
                    default:
                        return std::nullopt;
                }
            },
            [](const auto&) -> std::optional<int> {
                return std::nullopt;
            }
        }, expr.node);
    }

    void collect_assigned_names_expr(const AST::Expr& expr, std::unordered_set<std::string>& names) {
        std::visit(overloaded{
            [&](const AST::AssignmentExpr& assign) {
                if (const auto* var = std::get_if<AST::VariableExpr>(&assign.left->node)) names.insert(var->name);
                else collect_assigned_names_expr(*assign.left, names);
                collect_assigned_names_expr(*assign.right, names);
            },
            [&](const AST::UnaryExpr& un) {
                collect_assigned_names_expr(*un.expr, names);
            },
            [&](const AST::BinaryExpr& bin) {
                collect_assigned_names_expr(*bin.left, names);
                collect_assigned_names_expr(*bin.right, names);
            },
            [&](const AST::GroupingExpr& grp) {
                collect_assigned_names_expr(*grp.expr, names);
            },
            [&](const AST::ArrayLiteralExpr& arr) {
                for (const auto& child : arr.expr_list) collect_assigned_names_expr(*child, names);
            },
            [&](const AST::RangeLiteralExpr& range) {
                collect_assigned_names_expr(*range.start, names);
                collect_assigned_names_expr(*range.end, names);
            },
            [&](const AST::ArrayIndexExpr& idx) {
                collect_assigned_names_expr(*idx.arr_expr, names);
                collect_assigned_names_expr(*idx.index_expr, names);
            },
            [&](const AST::FunctionCallExpr& call) {
                collect_assigned_names_expr(*call.func_expr, names);
                for (const auto& arg : call.call_expr_list) collect_assigned_names_expr(*arg, names);
            },
            [](const auto&) {}
        }, expr.node);
    }

    void collect_assigned_names_stmt(const AST::Stmt& stmt, std::unordered_set<std::string>& names) {
        std::visit(overloaded{
            [&](const AST::ExprStmt& st) {
                collect_assigned_names_expr(*st.expr, names);
            },
            [&](const AST::BlockStmt& st) {
                for (const auto& child : st.stmt_list) collect_assigned_names_stmt(*child, names);
            },
            [&](const AST::VarDeclStmt& st) {
                if (st.init) collect_assigned_names_expr(*st.init, names);
            },
            [&](const AST::IOPrintStmt& st) {
                for (const auto& expr : st.expr_list) collect_assigned_names_expr(*expr, names);
            },
            [&](const AST::IOPrintlnStmt& st) {
                for (const auto& expr : st.expr_list) collect_assigned_names_expr(*expr, names);
            },
            [&](const AST::IOReadStmt& st) {
                for (const auto& name : st.names) names.insert(name);
            },
            [&](const AST::IfElseStmt& st) {
                collect_assigned_names_expr(*st.condition, names);
                collect_assigned_names_stmt(*st.then_branch, names);
                if (st.else_branch) collect_assigned_names_stmt(*st.else_branch, names);
            },
            [&](const AST::FuncDeclStmt&) {},
            [&](const AST::ReturnStmt& st) {
                if (st.expr) collect_assigned_names_expr(*st.expr, names);
            },
            [&](const AST::ForLoopStmt& st) {
                collect_assigned_names_expr(*st.iterable, names);
                collect_assigned_names_stmt(*st.body, names);
            },
            [&](const AST::WhileLoopStmt& st) {
                collect_assigned_names_expr(*st.cond, names);
                collect_assigned_names_stmt(*st.body, names);
            },
            [&](const AST::BreakStmt&) {},
            [&](const AST::ContinueStmt&) {},
            [&](const AST::ExitStmt& st) {
                collect_assigned_names_expr(*st.expr, names);
            }
        }, stmt.node);
    }

    class FunctionBuilder {
    private:
        VexIR::Function function;
        BindingMap bindings;
        ScopeStack scopes;
        std::vector<LoopFrame> loops;
        std::optional<size_t> current_block_index;
        int& temp_counter;
        int& block_counter;
        int& storage_counter;

        [[nodiscard]] VexIR::BasicBlock& current_block() {
            if (!current_block_index) throw VexIRGenException("VexIR lowering error: Attempted to emit into an unreachable block.");
            return function.blocks[*current_block_index];
        }

        [[nodiscard]] const VexIR::BasicBlock& current_block() const {
            if (!current_block_index) throw VexIRGenException("VexIR lowering error: Attempted to inspect an unreachable block.");
            return function.blocks[*current_block_index];
        }

        [[nodiscard]] static std::string escape_string(const std::string& value) {
            std::string escaped;
            for (const char ch : value) {
                switch (ch) {
                    case '\n': escaped += "\\n"; break;
                    case '\t': escaped += "\\t"; break;
                    case '\r': escaped += "\\r"; break;
                    case '"': escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    default: escaped += ch; break;
                }
            }
            return escaped;
        }

        [[nodiscard]] VexIR::IRType temp_type_from_ast(const AST::VarTypeSpec& type) const {
            return VexIR::scalar_type_from_ast(type);
        }

        [[nodiscard]] VexIR::IRType vector_type_from_scalar(const AST::VarTypeSpec& type) const {
            return VexIR::IRType{type.type, false, true, 2};
        }

        [[nodiscard]] VexIR::Temporary fresh_temp(const AST::VarTypeSpec& type, const std::string& prefix = "t") {
            return VexIR::Temporary{prefix + std::to_string(temp_counter++), temp_type_from_ast(type)};
        }

        [[nodiscard]] VexIR::Temporary fresh_vector_temp(const AST::VarTypeSpec& type, const std::string& prefix = "v") {
            return VexIR::Temporary{prefix + std::to_string(temp_counter++), vector_type_from_scalar(type)};
        }

        [[nodiscard]] VexIR::Storage fresh_storage(const AST::VarTypeSpec& type, const std::string& prefix = "x") {
            return VexIR::Storage{prefix + std::to_string(storage_counter++), type};
        }

        [[nodiscard]] std::string fresh_block_name(const std::string& hint) {
            return hint + "_" + std::to_string(block_counter++);
        }

        size_t add_block(const std::string& name) {
            function.blocks.push_back(VexIR::BasicBlock{name, {}, {}, std::nullopt});
            return function.blocks.size() - 1;
        }

        size_t add_named_block_and_switch(const std::string& name) {
            const size_t index = add_block(name);
            current_block_index = index;
            return index;
        }

        size_t find_block_index(const std::string& name) {
            for (size_t i = 0; i < function.blocks.size(); ++i) {
                if (function.blocks[i].name == name) return i;
            }
            throw VexIRGenException("VexIR lowering error: Internal block lookup failed for " + name + ".");
        }

        void emit(const VexIR::Instruction& instruction) {
            if (!current_block_index) return;
            auto& block = current_block();
            if (block.terminator) {
                throw VexIRGenException("VexIR lowering error: Tried to emit instruction after terminator in block " + block.name + ".");
            }
            block.instructions.push_back(instruction);
        }

        void set_terminator(const VexIR::Terminator& terminator) {
            if (!current_block_index) return;
            auto& block = current_block();
            if (block.terminator) {
                throw VexIRGenException("VexIR lowering error: Block " + block.name + " already has a terminator.");
            }
            block.terminator = terminator;
        }

        void push_scope() {
            scopes.emplace_back();
        }

        void declare_binding(const std::string& name, const Binding& binding) {
            if (scopes.empty()) push_scope();
            auto previous = bindings.contains(name) ? std::optional<Binding>(bindings.at(name)) : std::nullopt;
            scopes.back().emplace_back(name, previous);
            bindings[name] = binding;
        }

        void pop_scope() {
            if (scopes.empty()) throw VexIRGenException("VexIR lowering error: Internal scope stack underflow.");
            for (auto it = scopes.back().rbegin(); it != scopes.back().rend(); ++it) {
                if (it->second) bindings[it->first] = *it->second;
                else bindings.erase(it->first);
            }
            scopes.pop_back();
        }

        Binding& get_binding(const std::string& name) {
            if (!bindings.contains(name)) {
                throw VexIRGenException("VexIR lowering error: Symbol " + name + " is missing during IR generation.");
            }
            return bindings.at(name);
        }

        [[nodiscard]] const Binding& get_binding(const std::string& name) const {
            if (!bindings.contains(name)) {
                throw VexIRGenException("VexIR lowering error: Symbol " + name + " is missing during IR generation.");
            }
            return bindings.at(name);
        }

        [[nodiscard]] static bool is_scalar_binding(const Binding& binding) {
            return binding.kind == Binding::Kind::SSA_VALUE && !binding.type.is_array;
        }

        [[nodiscard]] static bool is_array_like_binding(const Binding& binding) {
            return binding.type.is_array;
        }

        [[nodiscard]] VexIR::Temporary require_scalar_value(const std::string& name) const {
            const auto& binding = get_binding(name);
            if (!is_scalar_binding(binding) || !binding.value) {
                throw VexIRGenException("VexIR lowering error: Expected scalar SSA value for symbol " + name + ".");
            }
            return *binding.value;
        }

        [[nodiscard]] VexIR::Temporary materialize_address(const Binding& binding) {
            if (!is_array_like_binding(binding)) {
                throw VexIRGenException("VexIR lowering error: Tried to materialize an address for a non-array symbol.");
            }
            if (binding.kind == Binding::Kind::ADDRESS_VALUE && binding.value) return *binding.value;
            if (binding.kind == Binding::Kind::STORAGE_OBJECT && binding.storage) {
                auto temp = fresh_temp(AST::VarTypeSpec{binding.type.type, true, binding.type.size}, "t");
                emit(VexIR::AddrOfInst{temp, *binding.storage});
                return temp;
            }
            throw VexIRGenException("VexIR lowering error: Array binding is missing address information.");
        }

        [[nodiscard]] VexIR::Storage ensure_addressable_scalar(const std::string& name) {
            auto& binding = get_binding(name);
            if (!is_scalar_binding(binding) || !binding.value) {
                throw VexIRGenException("VexIR lowering error: read(" + name + ") requires a mutable scalar variable.");
            }
            if (!binding.spill_storage) {
                binding.spill_storage = fresh_storage(binding.type);
                emit(VexIR::AllocLocalInst{*binding.spill_storage});
                emit(VexIR::StoreInst{*binding.spill_storage, *binding.value});
            }
            return *binding.spill_storage;
        }

        void set_scalar_binding(const std::string& name, const VexIR::Temporary& value) {
            auto& binding = get_binding(name);
            if (!is_scalar_binding(binding)) {
                throw VexIRGenException("VexIR lowering error: Cannot assign scalar SSA value to non-scalar symbol " + name + ".");
            }
            binding.value = value;
        }

        [[nodiscard]] VexIR::Temporary emit_default_scalar(const AST::VarTypeSpec& type) {
            auto temp = fresh_temp(type);
            if (type.type == AST::VarTypeOpts::INT32) emit(VexIR::ConstI32Inst{temp, 0});
            else emit(VexIR::ConstBoolInst{temp, false});
            return temp;
        }

        [[nodiscard]] int require_materialized_array_size(const AST::VarTypeSpec& type, const std::unique_ptr<AST::Expr>& init) {
            if (type.size) return *type.size;
            if (!init) {
                throw VexIRGenException("VexIR lowering error: Array declaration needs either an explicit size or an initializer.");
            }
            return std::visit(overloaded{
                [](const AST::ArrayLiteralExpr& arr) -> int {
                    return static_cast<int>(arr.expr_list.size());
                },
                [&](const AST::RangeLiteralExpr& range) -> int {
                    const auto start = try_eval_const_i32(*range.start);
                    const auto end = try_eval_const_i32(*range.end);
                    if (!start || !end) {
                        throw VexIRGenException("VexIR lowering error: Materialized range arrays need compile-time constant bounds.");
                    }
                    return (*end - *start) + 1;
                },
                [](const auto&) -> int {
                    throw VexIRGenException("VexIR lowering error: Could not infer array size from initializer.");
                }
            }, init->node);
        }

        [[nodiscard]] VexIR::Temporary lower_expr(const AST::Expr& expr) {
            if (!expr.inferred_type) throw VexIRGenException("VexIR lowering error: Encountered untyped expression during IR generation.");
            return std::visit(overloaded{
                [&](const AST::BasicLiteralExpr& lit) -> VexIR::Temporary {
                    auto temp = fresh_temp(*expr.inferred_type);
                    switch (lit.value.type) {
                        case TokenType::NUMBER:
                            emit(VexIR::ConstI32Inst{temp, std::stoi(lit.value.lexeme)});
                            break;
                        case TokenType::TRUE:
                            emit(VexIR::ConstBoolInst{temp, true});
                            break;
                        case TokenType::FALSE:
                            emit(VexIR::ConstBoolInst{temp, false});
                            break;
                        case TokenType::STRING:
                            emit(VexIR::ConstStrInst{temp, escape_string(lit.value.lexeme)});
                            break;
                        default:
                            throw VexIRGenException("VexIR lowering error: Unsupported literal token in expression.");
                    }
                    return temp;
                },
                [&](const AST::VariableExpr& var) -> VexIR::Temporary {
                    const auto& binding = get_binding(var.name);
                    if (is_scalar_binding(binding) && binding.value) return *binding.value;
                    if (is_array_like_binding(binding)) return materialize_address(binding);
                    throw VexIRGenException("VexIR lowering error: Symbol " + var.name + " does not have a usable value binding.");
                },
                [&](const AST::GroupingExpr& grp) -> VexIR::Temporary {
                    return lower_expr(*grp.expr);
                },
                [&](const AST::UnaryExpr& un) -> VexIR::Temporary {
                    auto src = lower_expr(*un.expr);
                    auto dest = fresh_temp(*expr.inferred_type);
                    emit(VexIR::UnaryInst{unary_op_from_token(un.op), dest, src});
                    return dest;
                },
                [&](const AST::BinaryExpr& bin) -> VexIR::Temporary {
                    auto lhs = lower_expr(*bin.left);
                    auto rhs = lower_expr(*bin.right);
                    auto dest = fresh_temp(*expr.inferred_type);
                    emit(VexIR::BinaryInst{binary_op_from_token(bin.op), dest, lhs, rhs});
                    return dest;
                },
                [&](const AST::AssignmentExpr& assign) -> VexIR::Temporary {
                    auto rhs = lower_expr(*assign.right);
                    if (const auto* var = std::get_if<AST::VariableExpr>(&assign.left->node)) {
                        auto& binding = get_binding(var->name);
                        if (is_scalar_binding(binding)) {
                            set_scalar_binding(var->name, rhs);
                            return rhs;
                        }
                        throw VexIRGenException("VexIR lowering error: Whole-array assignment is not supported in VexIR lowering yet for symbol " + var->name + ".");
                    }
                    if (const auto* idx = std::get_if<AST::ArrayIndexExpr>(&assign.left->node)) {
                        auto base = lower_expr(*idx->arr_expr);
                        auto index = lower_expr(*idx->index_expr);
                        auto ptr_type = AST::VarTypeSpec{expr.inferred_type->type, false, std::nullopt};
                        auto ptr = fresh_temp(AST::VarTypeSpec{ptr_type.type, false, ptr_type.size});
                        ptr.type.is_address = true;
                        emit(VexIR::ElemAddrInst{ptr, base, index});
                        emit(VexIR::StoreInst{ptr, rhs});
                        return rhs;
                    }
                    throw VexIRGenException("VexIR lowering error: Assignment target is not supported.");
                },
                [&](const AST::ArrayIndexExpr& idx) -> VexIR::Temporary {
                    auto base = lower_expr(*idx.arr_expr);
                    auto index = lower_expr(*idx.index_expr);
                    auto addr = fresh_temp(AST::VarTypeSpec{expr.inferred_type->type, false, std::nullopt});
                    addr.type.is_address = true;
                    emit(VexIR::ElemAddrInst{addr, base, index});
                    auto dest = fresh_temp(*expr.inferred_type);
                    emit(VexIR::LoadInst{dest, addr});
                    return dest;
                },
                [&](const AST::FunctionCallExpr& call) -> VexIR::Temporary {
                    if (!std::holds_alternative<AST::VariableExpr>(call.func_expr->node)) {
                        throw VexIRGenException("VexIR lowering error: Function call target must lower from a direct function name.");
                    }
                    const auto& func_name = std::get<AST::VariableExpr>(call.func_expr->node).name;
                    std::vector<VexIR::Temporary> args;
                    args.reserve(call.call_expr_list.size());
                    for (const auto& arg : call.call_expr_list) args.push_back(lower_expr(*arg));
                    auto dest = fresh_temp(*expr.inferred_type);
                    emit(VexIR::CallInst{dest, func_name, std::move(args)});
                    return dest;
                },
                [&](const AST::ArrayLiteralExpr& arr) -> VexIR::Temporary {
                    const int size = static_cast<int>(arr.expr_list.size());
                    if (size <= 0) throw VexIRGenException("VexIR lowering error: Empty array literal cannot be lowered.");
                    auto storage = fresh_storage(AST::VarTypeSpec{expr.inferred_type->type, true, size}, "a");
                    emit(VexIR::AllocArrayInst{storage, size});
                    for (int i = 0; i < size; ++i) {
                        auto idx_temp = fresh_temp(AST::VarTypeSpec{AST::VarTypeOpts::INT32, false, std::nullopt});
                        emit(VexIR::ConstI32Inst{idx_temp, i});
                        auto elem_ptr = fresh_temp(AST::VarTypeSpec{expr.inferred_type->type, false, std::nullopt});
                        elem_ptr.type.is_address = true;
                        emit(VexIR::ElemAddrInst{elem_ptr, storage, idx_temp});
                        auto elem_value = lower_expr(*arr.expr_list[static_cast<size_t>(i)]);
                        emit(VexIR::StoreInst{elem_ptr, elem_value});
                    }
                    auto addr = fresh_temp(*expr.inferred_type);
                    emit(VexIR::AddrOfInst{addr, storage});
                    return addr;
                },
                [&](const AST::RangeLiteralExpr& range) -> VexIR::Temporary {
                    const auto start = try_eval_const_i32(*range.start);
                    const auto end = try_eval_const_i32(*range.end);
                    if (!start || !end) {
                        throw VexIRGenException("VexIR lowering error: Materialized range arrays need compile-time constant bounds.");
                    }
                    if (*end < *start) {
                        throw VexIRGenException("VexIR lowering error: Range literal end is smaller than start.");
                    }
                    const int size = (*end - *start) + 1;
                    auto storage = fresh_storage(AST::VarTypeSpec{expr.inferred_type->type, true, size}, "a");
                    emit(VexIR::AllocArrayInst{storage, size});
                    for (int i = 0; i < size; ++i) {
                        auto idx_temp = fresh_temp(AST::VarTypeSpec{AST::VarTypeOpts::INT32, false, std::nullopt});
                        emit(VexIR::ConstI32Inst{idx_temp, i});
                        auto elem_ptr = fresh_temp(AST::VarTypeSpec{expr.inferred_type->type, false, std::nullopt});
                        elem_ptr.type.is_address = true;
                        emit(VexIR::ElemAddrInst{elem_ptr, storage, idx_temp});
                        auto value_temp = fresh_temp(AST::VarTypeSpec{AST::VarTypeOpts::INT32, false, std::nullopt});
                        emit(VexIR::ConstI32Inst{value_temp, *start + i});
                        emit(VexIR::StoreInst{elem_ptr, value_temp});
                    }
                    auto addr = fresh_temp(*expr.inferred_type);
                    emit(VexIR::AddrOfInst{addr, storage});
                    return addr;
                }
            }, expr.node);
        }

        void merge_into_current_block(const std::vector<std::pair<std::string, BindingMap>>& predecessors) {
            if (predecessors.empty()) {
                current_block_index = std::nullopt;
                return;
            }
            if (predecessors.size() == 1) {
                bindings = predecessors.front().second;
                return;
            }

            BindingMap merged = predecessors.front().second;
            std::unordered_set<std::string> names;
            for (const auto& [_, map] : predecessors) {
                for (const auto& [name, _binding] : map) names.insert(name);
            }

            for (const auto& name : names) {
                std::vector<const Binding*> incoming_bindings;
                incoming_bindings.reserve(predecessors.size());
                bool available_everywhere = true;
                for (const auto& [_, map] : predecessors) {
                    if (!map.contains(name)) {
                        available_everywhere = false;
                        break;
                    }
                    incoming_bindings.push_back(&map.at(name));
                }
                if (!available_everywhere || incoming_bindings.empty()) continue;

                if (std::all_of(incoming_bindings.begin(), incoming_bindings.end(),
                    [&](const Binding* candidate) { return bindings_equal(*candidate, *incoming_bindings.front()); })) {
                    merged[name] = *incoming_bindings.front();
                    continue;
                }

                if (!is_scalar_binding(*incoming_bindings.front())) {
                    throw VexIRGenException("VexIR lowering error: Control-flow merge for non-scalar symbol " + name +
                        " requires a representation that is not implemented.");
                }

                auto phi_dest = fresh_temp(incoming_bindings.front()->type);
                VexIR::PhiInst phi{phi_dest, {}};
                for (const auto& [pred_name, map] : predecessors) {
                    phi.incomings.push_back(VexIR::IncomingEdge{pred_name, *map.at(name).value});
                }
                current_block().phis.push_back(phi);
                Binding phi_binding = *incoming_bindings.front();
                phi_binding.value = phi_dest;
                merged[name] = phi_binding;
            }

            bindings = std::move(merged);
        }

        void lower_stmt(const AST::Stmt& stmt) {
            if (!current_block_index) return;
            std::visit(overloaded{
                [&](const AST::ExprStmt& st) {
                    static_cast<void>(lower_expr(*st.expr));
                },
                [&](const AST::BlockStmt& st) {
                    if (!st.special_dont_change_scope) push_scope();
                    for (const auto& child : st.stmt_list) {
                        if (!current_block_index) break;
                        lower_stmt(*child);
                    }
                    if (!st.special_dont_change_scope) pop_scope();
                },
                [&](const AST::VarDeclStmt& st) {
                    if (!st.type.is_array) {
                        VexIR::Temporary value = st.init ? lower_expr(*st.init) : emit_default_scalar(st.type);
                        declare_binding(st.name, Binding{Binding::Kind::SSA_VALUE, st.type, value, std::nullopt, std::nullopt});
                        return;
                    }

                    const int size = require_materialized_array_size(st.type, st.init);
                    AST::VarTypeSpec concrete_type = st.type;
                    concrete_type.size = size;
                    auto storage = fresh_storage(concrete_type, "a");
                    emit(VexIR::AllocArrayInst{storage, size});
                    declare_binding(st.name, Binding{Binding::Kind::STORAGE_OBJECT, concrete_type, std::nullopt, storage, std::nullopt});

                    if (!st.init) return;
                    std::visit(overloaded{
                        [&](const AST::ArrayLiteralExpr& arr) {
                            for (size_t i = 0; i < arr.expr_list.size(); ++i) {
                                auto idx_temp = fresh_temp(AST::VarTypeSpec{AST::VarTypeOpts::INT32, false, std::nullopt});
                                emit(VexIR::ConstI32Inst{idx_temp, static_cast<int>(i)});
                                auto ptr = fresh_temp(AST::VarTypeSpec{st.type.type, false, std::nullopt});
                                ptr.type.is_address = true;
                                emit(VexIR::ElemAddrInst{ptr, storage, idx_temp});
                                auto elem = lower_expr(*arr.expr_list[i]);
                                emit(VexIR::StoreInst{ptr, elem});
                            }
                        },
                        [&](const AST::RangeLiteralExpr& range) {
                            const auto start = try_eval_const_i32(*range.start);
                            const auto end = try_eval_const_i32(*range.end);
                            if (!start || !end) {
                                throw VexIRGenException("VexIR lowering error: Materialized range arrays need compile-time constant bounds.");
                            }
                            int current_value = *start;
                            for (int i = 0; i < size; ++i, ++current_value) {
                                auto idx_temp = fresh_temp(AST::VarTypeSpec{AST::VarTypeOpts::INT32, false, std::nullopt});
                                emit(VexIR::ConstI32Inst{idx_temp, i});
                                auto ptr = fresh_temp(AST::VarTypeSpec{st.type.type, false, std::nullopt});
                                ptr.type.is_address = true;
                                emit(VexIR::ElemAddrInst{ptr, storage, idx_temp});
                                auto value_temp = fresh_temp(AST::VarTypeSpec{AST::VarTypeOpts::INT32, false, std::nullopt});
                                emit(VexIR::ConstI32Inst{value_temp, current_value});
                                emit(VexIR::StoreInst{ptr, value_temp});
                            }
                        },
                        [&](const auto&) {
                            throw VexIRGenException("VexIR lowering error: Array variable " + st.name +
                                " needs an array-style initializer.");
                        }
                    }, st.init->node);
                },
                [&](const AST::IOPrintStmt& st) {
                    std::vector<VexIR::Temporary> args;
                    args.reserve(st.expr_list.size());
                    for (const auto& expr : st.expr_list) args.push_back(lower_expr(*expr));
                    emit(VexIR::PrintInst{false, std::move(args)});
                },
                [&](const AST::IOPrintlnStmt& st) {
                    std::vector<VexIR::Temporary> args;
                    args.reserve(st.expr_list.size());
                    for (const auto& expr : st.expr_list) args.push_back(lower_expr(*expr));
                    emit(VexIR::PrintInst{true, std::move(args)});
                },
                [&](const AST::IOReadStmt& st) {
                    std::vector<VexIR::Storage> targets;
                    targets.reserve(st.names.size());
                    for (const auto& name : st.names) targets.push_back(ensure_addressable_scalar(name));
                    emit(VexIR::ReadInst{targets});
                    for (size_t i = 0; i < st.names.size(); ++i) {
                        const auto& name = st.names[i];
                        auto loaded = fresh_temp(get_binding(name).type);
                        emit(VexIR::LoadInst{loaded, targets[i]});
                        set_scalar_binding(name, loaded);
                    }
                },
                [&](const AST::IfElseStmt& st) {
                    const auto parent_bindings = bindings;
                    const auto parent_scopes = scopes;
                    const auto then_name = fresh_block_name("then");
                    const auto else_name = fresh_block_name("else");
                    const auto merge_name = fresh_block_name("if_merge");
                    const auto then_index = add_block(then_name);
                    const auto else_index = add_block(else_name);
                    const auto merge_index = add_block(merge_name);
                    const auto cond = lower_expr(*st.condition);
                    set_terminator(VexIR::BranchTerminator{cond, then_name, else_name});

                    std::vector<std::pair<std::string, BindingMap>> predecessors;

                    current_block_index = then_index;
                    bindings = parent_bindings;
                    scopes = parent_scopes;
                    lower_stmt(*st.then_branch);
                    if (current_block_index) {
                        const auto pred_name = current_block().name;
                        set_terminator(VexIR::JumpTerminator{merge_name});
                        predecessors.emplace_back(pred_name, bindings);
                    }

                    current_block_index = else_index;
                    bindings = parent_bindings;
                    scopes = parent_scopes;
                    if (st.else_branch) lower_stmt(*st.else_branch);
                    if (current_block_index) {
                        const auto pred_name = current_block().name;
                        set_terminator(VexIR::JumpTerminator{merge_name});
                        predecessors.emplace_back(pred_name, bindings);
                    }

                    current_block_index = merge_index;
                    bindings = parent_bindings;
                    scopes = parent_scopes;
                    merge_into_current_block(predecessors);
                },
                [&](const AST::FuncDeclStmt&) {},
                [&](const AST::ReturnStmt& st) {
                    if (st.expr) set_terminator(VexIR::ReturnTerminator{lower_expr(*st.expr)});
                    else set_terminator(VexIR::ReturnTerminator{std::nullopt});
                    current_block_index = std::nullopt;
                },
                [&](const AST::WhileLoopStmt& st) {
                    const auto before_bindings = bindings;
                    const auto before_scopes = scopes;
                    const auto preheader_name = current_block().name;
                    std::unordered_set<std::string> assigned;
                    collect_assigned_names_stmt(*st.body, assigned);
                    std::vector<std::string> carried_names;
                    for (const auto& name : assigned) {
                        if (before_bindings.contains(name) && is_scalar_binding(before_bindings.at(name))) carried_names.push_back(name);
                    }

                    const auto cond_name = fresh_block_name("while_cond");
                    const auto body_name = fresh_block_name("while_body");
                    const auto exit_name = fresh_block_name("while_exit");
                    const auto cond_index = add_block(cond_name);
                    const auto body_index = add_block(body_name);
                    const auto exit_index = add_block(exit_name);

                    if (current_block_index) set_terminator(VexIR::JumpTerminator{cond_name});

                    current_block_index = cond_index;
                    bindings = before_bindings;
                    scopes = before_scopes;
                    std::unordered_map<std::string, size_t> phi_indices;
                    for (const auto& name : carried_names) {
                        auto phi_dest = fresh_temp(before_bindings.at(name).type);
                        current_block().phis.push_back(VexIR::PhiInst{phi_dest, {{preheader_name, before_bindings.at(name).value.value()}}});
                        phi_indices[name] = current_block().phis.size() - 1;
                        Binding phi_binding = before_bindings.at(name);
                        phi_binding.value = phi_dest;
                        bindings[name] = phi_binding;
                    }
                    const auto header_bindings = bindings;

                    const auto cond_value = lower_expr(*st.cond);
                    set_terminator(VexIR::BranchTerminator{cond_value, body_name, exit_name});

                    current_block_index = body_index;
                    bindings = header_bindings;
                    scopes = before_scopes;
                    loops.push_back(LoopFrame{cond_name, exit_name, std::unordered_set<std::string>(carried_names.begin(), carried_names.end()), {}, {}});
                    lower_stmt(*st.body);
                    LoopFrame loop = loops.back();
                    loops.pop_back();
                    if (current_block_index) {
                        loop.continue_states.emplace_back(current_block().name, bindings);
                        set_terminator(VexIR::JumpTerminator{cond_name});
                    }

                    auto& cond_block_ref = function.blocks[cond_index];
                    for (const auto& name : carried_names) {
                        auto& phi = cond_block_ref.phis[phi_indices.at(name)];
                        for (const auto& [pred_name, state] : loop.continue_states) {
                            phi.incomings.push_back(VexIR::IncomingEdge{pred_name, state.at(name).value.value()});
                        }
                    }

                    current_block_index = exit_index;
                    bindings = header_bindings;
                    scopes = before_scopes;
                    std::vector<std::pair<std::string, BindingMap>> exit_predecessors;
                    exit_predecessors.emplace_back(cond_name, header_bindings);
                    for (const auto& break_state : loop.break_states) exit_predecessors.push_back(break_state);
                    merge_into_current_block(exit_predecessors);
                },
                [&](const AST::ForLoopStmt& st) {
                    const auto before_bindings = bindings;
                    const auto before_scopes = scopes;
                    const auto preheader_name = current_block().name;
                    std::unordered_set<std::string> assigned;
                    collect_assigned_names_stmt(*st.body, assigned);
                    assigned.erase(st.iter_name);
                    std::vector<std::string> carried_names;
                    for (const auto& name : assigned) {
                        if (before_bindings.contains(name) && is_scalar_binding(before_bindings.at(name))) carried_names.push_back(name);
                    }

                    const auto cond_name = fresh_block_name("for_cond");
                    const auto body_name = fresh_block_name("for_body");
                    const auto exit_name = fresh_block_name("for_exit");
                    const auto cond_index = add_block(cond_name);
                    const auto body_index = add_block(body_name);
                    const auto exit_index = add_block(exit_name);

                    AST::VarTypeSpec i32_type{AST::VarTypeOpts::INT32, false, std::nullopt};
                    VexIR::Temporary start_temp{};
                    VexIR::Temporary end_temp{};
                    std::optional<VexIR::Temporary> array_addr;
                    bool iterates_range = false;

                    if (const auto* range = std::get_if<AST::RangeLiteralExpr>(&st.iterable->node)) {
                        iterates_range = true;
                        start_temp = lower_expr(*range->start);
                        end_temp = lower_expr(*range->end);
                    } else {
                        std::optional<int> iterable_size = st.iterable->inferred_type ? st.iterable->inferred_type->size : std::nullopt;
                        if (!iterable_size) {
                            if (const auto* var = std::get_if<AST::VariableExpr>(&st.iterable->node)) {
                                if (before_bindings.contains(var->name) && before_bindings.at(var->name).type.size) {
                                    iterable_size = before_bindings.at(var->name).type.size;
                                }
                            }
                        }
                        if (!iterable_size) {
                            throw VexIRGenException("VexIR lowering error: Array-based for loops need a statically known array size.");
                        }
                        array_addr = lower_expr(*st.iterable);
                        start_temp = fresh_temp(i32_type);
                        emit(VexIR::ConstI32Inst{start_temp, 0});
                        end_temp = fresh_temp(i32_type);
                        emit(VexIR::ConstI32Inst{end_temp, *iterable_size - 1});
                    }

                    if (current_block_index) set_terminator(VexIR::JumpTerminator{cond_name});

                    current_block_index = cond_index;
                    bindings = before_bindings;
                    scopes = before_scopes;
                    std::unordered_map<std::string, size_t> phi_indices;

                    const auto iter_phi = fresh_temp(i32_type);
                    current_block().phis.push_back(VexIR::PhiInst{iter_phi, {{preheader_name, start_temp}}});
                    for (const auto& name : carried_names) {
                        auto phi_dest = fresh_temp(before_bindings.at(name).type);
                        current_block().phis.push_back(VexIR::PhiInst{phi_dest, {{preheader_name, before_bindings.at(name).value.value()}}});
                        phi_indices[name] = current_block().phis.size() - 1;
                        Binding phi_binding = before_bindings.at(name);
                        phi_binding.value = phi_dest;
                        bindings[name] = phi_binding;
                    }
                    const auto header_bindings = bindings;

                    auto cmp_temp = fresh_temp(AST::VarTypeSpec{AST::VarTypeOpts::BOOL, false, std::nullopt});
                    emit(VexIR::BinaryInst{VexIR::BinaryOp::LEQ, cmp_temp, iter_phi, end_temp});
                    set_terminator(VexIR::BranchTerminator{cmp_temp, body_name, exit_name});

                    current_block_index = body_index;
                    bindings = header_bindings;
                    scopes = before_scopes;
                    push_scope();
                    if (iterates_range) {
                        declare_binding(st.iter_name, Binding{Binding::Kind::SSA_VALUE, i32_type, iter_phi, std::nullopt, std::nullopt});
                    } else {
                        auto elem_ptr = fresh_temp(AST::VarTypeSpec{st.iterable->inferred_type->type, false, std::nullopt});
                        elem_ptr.type.is_address = true;
                        emit(VexIR::ElemAddrInst{elem_ptr, *array_addr, iter_phi});
                        auto elem_value = fresh_temp(AST::VarTypeSpec{st.iterable->inferred_type->type, false, std::nullopt});
                        emit(VexIR::LoadInst{elem_value, elem_ptr});
                        declare_binding(st.iter_name, Binding{Binding::Kind::SSA_VALUE, AST::VarTypeSpec{st.iterable->inferred_type->type, false, std::nullopt}, elem_value, std::nullopt, std::nullopt});
                    }

                    loops.push_back(LoopFrame{cond_name, exit_name, std::unordered_set<std::string>(carried_names.begin(), carried_names.end()), {}, {}});
                    lower_stmt(*st.body);
                    LoopFrame loop = loops.back();
                    loops.pop_back();
                    pop_scope();

                    if (current_block_index) {
                        loop.continue_states.emplace_back(current_block().name, bindings);
                        auto one = fresh_temp(i32_type);
                        emit(VexIR::ConstI32Inst{one, 1});
                        auto iter_next = fresh_temp(i32_type);
                        emit(VexIR::BinaryInst{VexIR::BinaryOp::ADD, iter_next, iter_phi, one});
                        set_terminator(VexIR::JumpTerminator{cond_name});
                        function.blocks[cond_index].phis[0].incomings.push_back(VexIR::IncomingEdge{loop.continue_states.back().first, iter_next});
                    }

                    for (const auto& name : carried_names) {
                        auto& phi = function.blocks[cond_index].phis[phi_indices.at(name)];
                        for (const auto& [pred_name, state] : loop.continue_states) {
                            phi.incomings.push_back(VexIR::IncomingEdge{pred_name, *state.at(name).value});
                        }
                    }

                    current_block_index = exit_index;
                    bindings = header_bindings;
                    scopes = before_scopes;
                    std::vector<std::pair<std::string, BindingMap>> exit_predecessors;
                    exit_predecessors.emplace_back(cond_name, header_bindings);
                    for (const auto& break_state : loop.break_states) exit_predecessors.push_back(break_state);
                    merge_into_current_block(exit_predecessors);
                },
                [&](const AST::BreakStmt&) {
                    if (loops.empty()) throw VexIRGenException("VexIR lowering error: break appeared without an active loop.");
                    loops.back().break_states.emplace_back(current_block().name, bindings);
                    set_terminator(VexIR::JumpTerminator{loops.back().exit_block});
                    current_block_index = std::nullopt;
                },
                [&](const AST::ContinueStmt&) {
                    if (loops.empty()) throw VexIRGenException("VexIR lowering error: continue appeared without an active loop.");
                    loops.back().continue_states.emplace_back(current_block().name, bindings);
                    set_terminator(VexIR::JumpTerminator{loops.back().cond_block});
                    current_block_index = std::nullopt;
                },
                [&](const AST::ExitStmt& st) {
                    set_terminator(VexIR::ExitTerminator{lower_expr(*st.expr)});
                    current_block_index = std::nullopt;
                }
            }, stmt.node);
        }

    public:
        FunctionBuilder(std::string name, std::vector<VexIR::Param> params, std::optional<VexIR::IRType> return_type,
                        bool is_entrypoint, int& temp_counter_ref, int& block_counter_ref, int& storage_counter_ref)
            : function{std::move(name), std::move(params), std::move(return_type), {}, is_entrypoint},
              temp_counter(temp_counter_ref), block_counter(block_counter_ref), storage_counter(storage_counter_ref) {}

        void initialize_entry() {
            push_scope();
            add_named_block_and_switch("entry");
        }

        void bind_scalar_param(const std::string& name, const AST::VarTypeSpec& type) {
            declare_binding(name, Binding{Binding::Kind::SSA_VALUE, type, VexIR::Temporary{name, temp_type_from_ast(type)}, std::nullopt, std::nullopt});
        }

        void bind_array_param(const std::string& name, const AST::VarTypeSpec& type) {
            auto temp = VexIR::Temporary{name, VexIR::scalar_type_from_ast(type)};
            temp.type.is_address = true;
            declare_binding(name, Binding{Binding::Kind::ADDRESS_VALUE, type, temp, std::nullopt, std::nullopt});
        }

        void lower_stmt_list(const std::vector<std::unique_ptr<AST::Stmt>>& stmts) {
            for (const auto& stmt : stmts) {
                if (!current_block_index) break;
                lower_stmt(*stmt);
            }
        }

        void lower_single_stmt(const AST::Stmt& stmt) {
            lower_stmt(stmt);
        }

        void finalize_entrypoint_return() {
            if (!current_block_index) return;
            AST::VarTypeSpec i32_type{AST::VarTypeOpts::INT32, false, std::nullopt};
            auto zero = fresh_temp(i32_type);
            emit(VexIR::ConstI32Inst{zero, 0});
            set_terminator(VexIR::ReturnTerminator{zero});
        }

        void finalize_function_return() {
            if (!current_block_index) return;
            if (!function.return_type) {
                set_terminator(VexIR::ReturnTerminator{std::nullopt});
                return;
            }
            throw VexIRGenException("VexIR lowering error: Function " + function.name + " can fall off the end without returning a value.");
        }

        [[nodiscard]] VexIR::Function take() {
            return std::move(function);
        }
    };

    std::vector<VexIR::Instruction> vectorize_block(const std::vector<VexIR::Instruction>& instructions) {
        std::vector<VexIR::Instruction> optimized;
        optimized.reserve(instructions.size());

        size_t i = 0;
        while (i < instructions.size()) {
            if (i + 1 < instructions.size()) {
                const auto* first = std::get_if<VexIR::BinaryInst>(&instructions[i]);
                const auto* second = std::get_if<VexIR::BinaryInst>(&instructions[i + 1]);
                if (first && second && first->op == second->op &&
                    VexIR::is_vectorizable_binary(first->op) &&
                    first->dest.type.base == second->dest.type.base &&
                    !first->dest.type.is_vector && !second->dest.type.is_vector &&
                    first->dest.name != second->lhs.name && first->dest.name != second->rhs.name &&
                    second->dest.name != first->lhs.name && second->dest.name != first->rhs.name) {
                    const AST::VarTypeSpec scalar_type{first->dest.type.base, false, std::nullopt};
                    VexIR::Temporary lhs_pack{"vslp_lhs_" + first->dest.name + "_" + second->dest.name, VexIR::IRType{scalar_type.type, false, true, 2}};
                    VexIR::Temporary rhs_pack{"vslp_rhs_" + first->dest.name + "_" + second->dest.name, VexIR::IRType{scalar_type.type, false, true, 2}};
                    VexIR::Temporary vec_result{"vslp_res_" + first->dest.name + "_" + second->dest.name, VexIR::IRType{scalar_type.type, false, true, 2}};
                    optimized.emplace_back(VexIR::Pack2Inst{lhs_pack, first->lhs, second->lhs});
                    optimized.emplace_back(VexIR::Pack2Inst{rhs_pack, first->rhs, second->rhs});
                    optimized.emplace_back(VexIR::VectorBinaryInst{VexIR::vector_op_from_binary(first->op), vec_result, lhs_pack, rhs_pack});
                    optimized.emplace_back(VexIR::ExtractInst{first->dest, vec_result, 0});
                    optimized.emplace_back(VexIR::ExtractInst{second->dest, vec_result, 1});
                    i += 2;
                    continue;
                }
            }
            optimized.push_back(instructions[i]);
            ++i;
        }

        return optimized;
    }

    VexIR::Module optimize_slp(const VexIR::Module& input) {
        VexIR::Module output = input;
        for (auto& function : output.functions) {
            for (auto& block : function.blocks) {
                block.instructions = vectorize_block(block.instructions);
            }
        }
        return output;
    }

    void prune_unreachable_blocks(VexIR::Function& function) {
        if (function.blocks.empty()) return;

        std::unordered_map<std::string, size_t> index_by_name;
        for (size_t i = 0; i < function.blocks.size(); ++i) index_by_name[function.blocks[i].name] = i;

        std::vector<bool> reachable(function.blocks.size(), false);
        std::vector<size_t> worklist{0};
        reachable[0] = true;

        while (!worklist.empty()) {
            const size_t current = worklist.back();
            worklist.pop_back();
            const auto& block = function.blocks[current];
            if (!block.terminator) continue;
            std::visit(overloaded{
                [&](const VexIR::JumpTerminator& term) {
                    if (index_by_name.contains(term.target) && !reachable[index_by_name.at(term.target)]) {
                        reachable[index_by_name.at(term.target)] = true;
                        worklist.push_back(index_by_name.at(term.target));
                    }
                },
                [&](const VexIR::BranchTerminator& term) {
                    for (const auto* target : {&term.true_target, &term.false_target}) {
                        if (index_by_name.contains(*target) && !reachable[index_by_name.at(*target)]) {
                            reachable[index_by_name.at(*target)] = true;
                            worklist.push_back(index_by_name.at(*target));
                        }
                    }
                },
                [&](const auto&) {}
            }, *block.terminator);
        }

        std::vector<VexIR::BasicBlock> kept;
        kept.reserve(function.blocks.size());
        for (size_t i = 0; i < function.blocks.size(); ++i) {
            if (reachable[i]) kept.push_back(function.blocks[i]);
        }
        function.blocks = std::move(kept);
    }
}

VexIRGenerator::VexIRGenerator(std::vector<std::unique_ptr<AST::Stmt>> p_ast) : ast(std::move(p_ast)) {}

void VexIRGenerator::build() {
    generated_ir = {};
    optimized_ir = {};
    is_error = false;

    try {
        int temp_counter = 0;
        int block_counter = 0;
        int storage_counter = 0;

        std::vector<const AST::FuncDeclStmt*> functions;
        for (const auto& stmt : ast) {
            if (const auto* func = std::get_if<AST::FuncDeclStmt>(&stmt->node)) functions.push_back(func);
        }

        FunctionBuilder main_builder("main", {}, VexIR::IRType{AST::VarTypeOpts::INT32, false, false, 1}, true,
                                     temp_counter, block_counter, storage_counter);
        main_builder.initialize_entry();
        for (const auto& stmt : ast) {
            if (std::holds_alternative<AST::FuncDeclStmt>(stmt->node)) continue;
            main_builder.lower_single_stmt(*stmt);
        }
        main_builder.finalize_entrypoint_return();
        generated_ir.functions.push_back(main_builder.take());

        for (const auto* func : functions) {
            std::vector<VexIR::Param> params;
            params.reserve(func->param_list.size());
            for (const auto& param : func->param_list) {
                auto param_type = VexIR::scalar_type_from_ast(param.type);
                if (param.type.is_array) param_type.is_address = true;
                params.push_back(VexIR::Param{param.name, param_type});
            }
            std::optional<VexIR::IRType> return_type;
            if (func->return_type) return_type = VexIR::scalar_type_from_ast(*func->return_type);

            FunctionBuilder builder(func->name, params, return_type, false, temp_counter, block_counter, storage_counter);
            builder.initialize_entry();
            for (const auto& param : func->param_list) {
                if (param.type.is_array) builder.bind_array_param(param.name, param.type);
                else builder.bind_scalar_param(param.name, param.type);
            }
            builder.lower_single_stmt(*func->body);
            builder.finalize_function_return();
            generated_ir.functions.push_back(builder.take());
        }

        for (auto& function : generated_ir.functions) prune_unreachable_blocks(function);
        optimized_ir = optimize_slp(generated_ir);
    } catch (const VexIRGenException& e) {
        is_error = true;
        generated_ir = {};
        optimized_ir = {};
        std::cerr << e.what() << "\n";
    }
}

std::string VexIRGenerator::print_ir() const {
    return VexIR::print_module(optimized_ir.functions.empty() ? generated_ir : optimized_ir);
}

std::string VexIRGenerator::print_raw_ir() const {
    return VexIR::print_module(generated_ir);
}

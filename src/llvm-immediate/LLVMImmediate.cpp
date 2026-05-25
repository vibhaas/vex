//
// Created by accord.
//

#include "llvm-immediate/LLVMImmediate.hpp"
#include <iostream>

LLVMImmediate::LLVMImmediate(std::vector<std::unique_ptr<AST::Stmt>> p_ast) : ast(std::move(p_ast)) {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("vexc_module", *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);

    symbols.emplace_back();
}

std::string LLVMImmediate::print_llvm() const {
    std::string ir_str;
    llvm::raw_string_ostream os(ir_str);
    module->print(os, nullptr);
    return ir_str;
}

void LLVMImmediate::build() {
    try {
        // first pass : register all functions
        for (const auto& stmt : ast) {
            if (auto* f = std::get_if<AST::FuncDeclStmt>(&stmt->node)) {
                std::vector<llvm::Type*> param_types;
                for (const auto& p : f->param_list) param_types.push_back(get_llvm_type(p.type));
                llvm::Type* ret_type = f->return_type ? get_llvm_type(*f->return_type) : llvm::Type::getVoidTy(*context);
                llvm::FunctionType* ft = llvm::FunctionType::get(ret_type, param_types, false);
                llvm::Function::Create(ft, llvm::Function::ExternalLinkage, f->name, module.get());
            }
        }

        // setup main func
        llvm::FunctionType* main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), false);
        llvm::Function* main_fn = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", module.get());
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*context, "entry", main_fn);
        builder->SetInsertPoint(entry_bb);

        // all top level, non-func statements into main
        for (const auto& stmt : ast) {
            if (std::holds_alternative<AST::FuncDeclStmt>(stmt->node)) continue;
            emit_stmt(*stmt);
        }

        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true)));
        }

        // now, emit all the functions
        for (const auto& stmt : ast) {
            if (std::holds_alternative<AST::FuncDeclStmt>(stmt->node)) emit_stmt(*stmt);
        }
    }
    catch (const LLVMImmException &e) {
        std::cerr << "LLVM (Immediate) Codegen Error : " << e.what() << "\n";
        is_error = true;
    }
    catch (const std::exception &e) {
        std::cerr << "CRITICAL LLVM (Immediate) Codegen Error : " << e.what() << "\n";
        is_error = true;
    }
}

// helper
llvm::Type* LLVMImmediate::get_llvm_type(const AST::VarTypeSpec& type_spec) const {
    llvm::Type* base_type = nullptr;
    switch (type_spec.type) {
        case AST::VarTypeOpts::INT32: base_type = llvm::Type::getInt32Ty(*context); break;
        case AST::VarTypeOpts::BOOL: base_type = llvm::Type::getInt1Ty(*context); break;
        case AST::VarTypeOpts::STRING_LITERAL: base_type = llvm::PointerType::getUnqual(*context); break;
    }
    if (type_spec.is_array) {
        if (type_spec.size.has_value()) return llvm::ArrayType::get(base_type, *type_spec.size);
        return llvm::PointerType::getUnqual(*context); // Dynamic arrays decay to opaque pointers
    }
    return base_type;
}

// helper
llvm::Value* LLVMImmediate::get_symbol_ptr(const std::string& name) const {
    for (int i = static_cast<int>(symbols.size()) - 1; i >= 0; --i) {
        if (symbols[i].contains(name)) return symbols[i].at(name);
    }
    throw LLVMImmException("Symbol " + name + " not found in scope during codegen.");
}

// expression
llvm::Value* LLVMImmediate::emit_expr(const AST::Expr& expr) {
    return std::visit(overloaded {
        [&](const AST::BasicLiteralExpr& exp) -> llvm::Value* {
            switch (exp.value.type) {
                case TokenType::NUMBER:
                    return llvm::ConstantInt::get(*context, llvm::APInt(32, std::stoi(exp.value.lexeme), true));
                case TokenType::TRUE:
                    return llvm::ConstantInt::getTrue(*context);
                case TokenType::FALSE:
                    return llvm::ConstantInt::getFalse(*context);
                case TokenType::STRING:
                    return builder->CreateGlobalStringPtr(exp.value.lexeme, "strtmp");
                default:
                    throw LLVMImmException("Invalid literal token encountered.");
            }
        },
        [&](const AST::ArrayLiteralExpr& exp) -> llvm::Value* {
            llvm::Type* elem_type = get_llvm_type({expr.inferred_type->type, false, std::nullopt});
            llvm::ArrayType* arr_type = llvm::ArrayType::get(elem_type, exp.expr_list.size());
            llvm::AllocaInst* alloca = builder->CreateAlloca(arr_type, nullptr, "arrlit");

            for (size_t i = 0; i < exp.expr_list.size(); ++i) {
                llvm::Value* val = emit_expr(*exp.expr_list[i]);
                llvm::Value* idx = llvm::ConstantInt::get(*context, llvm::APInt(32, i, true));
                llvm::Value* zero = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true));
                llvm::Value* gep = builder->CreateInBoundsGEP(arr_type, alloca, {zero, idx});
                builder->CreateStore(val, gep);
            }
            return alloca;
        },
        [&](const AST::RangeLiteralExpr&) -> llvm::Value* {
            throw LLVMImmException("Range literals are only valid within ForLoopStmt bounds.");
        },
        [&](const AST::VariableExpr& exp) -> llvm::Value* {
            llvm::Value* ptr = get_symbol_ptr(exp.name);
            if (expr.inferred_type->is_array) return ptr;
            return builder->CreateLoad(get_llvm_type(*expr.inferred_type), ptr, exp.name + "_ld");
        },
        [&](const AST::ArrayIndexExpr& exp) -> llvm::Value* {
            llvm::Value* arr_ptr = nullptr;
            if (auto* v = std::get_if<AST::VariableExpr>(&exp.arr_expr->node)) {
                arr_ptr = get_symbol_ptr(v->name);
            } else {
                throw LLVMImmException("Complex array indexing not yet supported.");
            }

            llvm::Value* idx = emit_expr(*exp.index_expr);
            llvm::Value* gep = nullptr;

            if (exp.arr_expr->inferred_type->size.has_value()) {
                llvm::Type* arr_type = get_llvm_type(*exp.arr_expr->inferred_type);
                llvm::Value* zero = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true));
                gep = builder->CreateInBoundsGEP(arr_type, arr_ptr, {zero, idx}, "idxgep");
            } else {
                llvm::Type* elem_type = get_llvm_type({exp.arr_expr->inferred_type->type, false, std::nullopt});
                gep = builder->CreateInBoundsGEP(elem_type, arr_ptr, idx, "idxgep");
            }

            return builder->CreateLoad(get_llvm_type(*expr.inferred_type), gep, "idxld");
        },
        [&](const AST::FunctionCallExpr& exp) -> llvm::Value* {
            const std::string func_name = std::get<AST::VariableExpr>(exp.func_expr->node).name;
            llvm::Function* callee = module->getFunction(func_name);
            if (!callee) throw LLVMImmException("Function " + func_name + " not found.");

            std::vector<llvm::Value*> args;
            for (const auto& arg : exp.call_expr_list) args.push_back(emit_expr(*arg));
            return builder->CreateCall(callee, args, expr.inferred_type ? "calltmp" : "");
        },
        [&](const AST::GroupingExpr& exp) -> llvm::Value* {
            return emit_expr(*exp.expr);
        },
        [&](const AST::UnaryExpr& exp) -> llvm::Value* {
            llvm::Value* val = emit_expr(*exp.expr);
            switch (exp.op) {
                case TokenType::MINUS: return builder->CreateNeg(val, "negtmp");
                case TokenType::NOT: return builder->CreateNot(val, "nottmp");
                default: throw LLVMImmException("Invalid unary operator.");
            }
        },
        [&](const AST::BinaryExpr& exp) -> llvm::Value* {
            llvm::Value* L = emit_expr(*exp.left);
            llvm::Value* R = emit_expr(*exp.right);
            switch (exp.op) {
                case TokenType::PLUS: return builder->CreateAdd(L, R, "addtmp");
                case TokenType::MINUS: return builder->CreateSub(L, R, "subtmp");
                case TokenType::MULTIPLY: return builder->CreateMul(L, R, "multmp");
                case TokenType::DIVIDE: return builder->CreateSDiv(L, R, "divtmp");
                case TokenType::MODULO: return builder->CreateSRem(L, R, "modtmp");
                case TokenType::GREATER: return builder->CreateICmpSGT(L, R, "gttmp");
                case TokenType::GREATER_EQUAL: return builder->CreateICmpSGE(L, R, "getmp");
                case TokenType::LESS: return builder->CreateICmpSLT(L, R, "lttmp");
                case TokenType::LESS_EQUAL: return builder->CreateICmpSLE(L, R, "letmp");
                case TokenType::EQUAL_EQUAL: return builder->CreateICmpEQ(L, R, "eqtmp");
                case TokenType::NOT_EQUAL: return builder->CreateICmpNE(L, R, "netmp");
                case TokenType::AND: return builder->CreateAnd(L, R, "andtmp");
                case TokenType::OR: return builder->CreateOr(L, R, "ortmp");
                default: throw LLVMImmException("Invalid binary operator.");
            }
        },
        [&](const AST::AssignmentExpr& exp) -> llvm::Value* {
            llvm::Value* rhs = emit_expr(*exp.right);
            llvm::Value* lhs_ptr = nullptr;

            if (auto* v = std::get_if<AST::VariableExpr>(&exp.left->node)) {
                lhs_ptr = get_symbol_ptr(v->name);
            } else if (auto* a = std::get_if<AST::ArrayIndexExpr>(&exp.left->node)) {
                if (auto* v_arr = std::get_if<AST::VariableExpr>(&a->arr_expr->node)) {
                    llvm::Value* arr_ptr = get_symbol_ptr(v_arr->name);
                    llvm::Value* idx = emit_expr(*a->index_expr);

                    if (a->arr_expr->inferred_type->size.has_value()) {
                        llvm::Type* arr_type = get_llvm_type(*a->arr_expr->inferred_type);
                        llvm::Value* zero = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true));
                        lhs_ptr = builder->CreateInBoundsGEP(arr_type, arr_ptr, {zero, idx}, "idxgep");
                    } else {
                        llvm::Type* elem_type = get_llvm_type({a->arr_expr->inferred_type->type, false, std::nullopt});
                        lhs_ptr = builder->CreateInBoundsGEP(elem_type, arr_ptr, idx, "idxgep");
                    }
                } else {
                    throw LLVMImmException("Complex array assignments not supported yet.");
                }
            } else {
                throw LLVMImmException("Invalid assignment left-hand side.");
            }

            builder->CreateStore(rhs, lhs_ptr);
            return rhs;
        }
    }, expr.node);
}

// statements

void LLVMImmediate::emit_stmt(const AST::Stmt& stmt) {
    std::visit(overloaded {
        [&](const AST::ExprStmt& st) -> void {
            emit_expr(*st.expr);
        },
        [&](const AST::BlockStmt& st) -> void {
            if (!st.special_dont_change_scope) symbols.emplace_back();
            for (const auto& child : st.stmt_list) emit_stmt(*child);
            if (!st.special_dont_change_scope) symbols.pop_back();
        },
        [&](const AST::VarDeclStmt& st) -> void {
            llvm::Type* type = get_llvm_type(st.type);
            llvm::AllocaInst* alloca = builder->CreateAlloca(type, nullptr, st.name);

            if (st.init) {
                builder->CreateStore(emit_expr(*st.init), alloca);
            } else {
                // on init, give a default val in case no provided
                llvm::Value* zero_val = nullptr;
                if (st.type.is_array && st.type.size.has_value()) {
                    zero_val = llvm::ConstantAggregateZero::get(type);
                } else if (st.type.type == AST::VarTypeOpts::INT32) {
                    zero_val = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true));
                } else if (st.type.type == AST::VarTypeOpts::BOOL) {
                    zero_val = llvm::ConstantInt::getFalse(*context);
                } else {
                    zero_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context));
                }
                builder->CreateStore(zero_val, alloca);
            }
            symbols.back()[st.name] = alloca;
        },
        [&](const AST::IOPrintStmt& st) -> void {
            llvm::Function* printf_fn = module->getFunction("printf");
            if (!printf_fn) {
                llvm::FunctionType* pft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), {llvm::PointerType::getUnqual(*context)}, true);
                printf_fn = llvm::Function::Create(pft, llvm::Function::ExternalLinkage, "printf", module.get());
            }
            std::string fmt;
            std::vector<llvm::Value*> args;
            args.push_back(nullptr); // format string placeholder

            for (const auto& ex : st.expr_list) {
                llvm::Value* val = emit_expr(*ex);
                if (ex->inferred_type->type == AST::VarTypeOpts::INT32) { fmt += "%d"; args.push_back(val); }
                else if (ex->inferred_type->type == AST::VarTypeOpts::STRING_LITERAL) { fmt += "%s"; args.push_back(val); }
                else if (ex->inferred_type->type == AST::VarTypeOpts::BOOL) {
                    fmt += "%s";
                    llvm::Value* ts = builder->CreateGlobalStringPtr("true", "t_str");
                    llvm::Value* fs = builder->CreateGlobalStringPtr("false", "f_str");
                    args.push_back(builder->CreateSelect(val, ts, fs, "bool_sel"));
                }
            }
            args[0] = builder->CreateGlobalStringPtr(fmt, "fmt");
            builder->CreateCall(printf_fn, args);
        },
        [&](const AST::IOPrintlnStmt& st) -> void {
            llvm::Function* printf_fn = module->getFunction("printf");
            if (!printf_fn) {
                llvm::FunctionType* pft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), {llvm::PointerType::getUnqual(*context)}, true);
                printf_fn = llvm::Function::Create(pft, llvm::Function::ExternalLinkage, "printf", module.get());
            }
            std::string fmt;
            std::vector<llvm::Value*> args;
            args.push_back(nullptr);

            for (const auto& ex : st.expr_list) {
                llvm::Value* val = emit_expr(*ex);
                if (ex->inferred_type->type == AST::VarTypeOpts::INT32) { fmt += "%d"; args.push_back(val); }
                else if (ex->inferred_type->type == AST::VarTypeOpts::STRING_LITERAL) { fmt += "%s"; args.push_back(val); }
                else if (ex->inferred_type->type == AST::VarTypeOpts::BOOL) {
                    fmt += "%s";
                    llvm::Value* ts = builder->CreateGlobalStringPtr("true", "t_str");
                    llvm::Value* fs = builder->CreateGlobalStringPtr("false", "f_str");
                    args.push_back(builder->CreateSelect(val, ts, fs, "bool_sel"));
                }
            }
            fmt += "\n";
            args[0] = builder->CreateGlobalStringPtr(fmt, "fmt");
            builder->CreateCall(printf_fn, args);
        },
        [&](const AST::IOReadStmt& st) -> void {
            llvm::Function* scanf_fn = module->getFunction("scanf");
            if (!scanf_fn) {
                llvm::FunctionType* sft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), {llvm::PointerType::getUnqual(*context)}, true);
                scanf_fn = llvm::Function::Create(sft, llvm::Function::ExternalLinkage, "scanf", module.get());
            }
            for (const auto& name : st.names) {
                // Right now we can only read %d (i32) types
                llvm::Value* ptr = get_symbol_ptr(name);
                builder->CreateCall(scanf_fn, {builder->CreateGlobalStringPtr("%d", "fmt"), ptr});
            }
        },
        [&](const AST::IfElseStmt& st) -> void {
            llvm::Value* cond = emit_expr(*st.condition);
            llvm::Function* func = builder->GetInsertBlock()->getParent();

            llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*context, "then", func);
            llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*context, "else");
            llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context, "ifcont");

            bool has_else = st.else_branch != nullptr;
            builder->CreateCondBr(cond, then_bb, has_else ? else_bb : merge_bb);

            // then
            builder->SetInsertPoint(then_bb);
            emit_stmt(*st.then_branch);
            bool then_needs_merge = !builder->GetInsertBlock()->getTerminator();
            if (then_needs_merge) builder->CreateBr(merge_bb);

            // else
            bool else_needs_merge = false;
            if (has_else) {
                func->insert(func->end(), else_bb);
                builder->SetInsertPoint(else_bb);
                emit_stmt(*st.else_branch);
                else_needs_merge = !builder->GetInsertBlock()->getTerminator();
                if (else_needs_merge) builder->CreateBr(merge_bb);
            } else {
                delete else_bb;
            }

            if (then_needs_merge || else_needs_merge || !has_else) {
                func->insert(func->end(), merge_bb);
                builder->SetInsertPoint(merge_bb);
            } else {
                delete merge_bb;
            }
        },
        [&](const AST::FuncDeclStmt& st) -> void {
            llvm::Function* func = module->getFunction(st.name);
            if (!func) throw LLVMImmException("Critical: Function " + st.name + " prototype missing.");

            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*context, "entry", func);
            builder->SetInsertPoint(bb);

            symbols.emplace_back(); // function scope

            size_t idx = 0;
            for (auto& arg : func->args()) {
                arg.setName(st.param_list[idx].name);
                llvm::Type* arg_type = func->getFunctionType()->getParamType(idx);
                llvm::AllocaInst* alloca = builder->CreateAlloca(arg_type, nullptr, arg.getName() + "_ptr");
                builder->CreateStore(&arg, alloca);
                symbols.back()[st.param_list[idx].name] = alloca;
                idx++;
            }

            emit_stmt(*st.body);

            if (!builder->GetInsertBlock()->getTerminator()) {
                if (!st.return_type) builder->CreateRetVoid();
                else throw LLVMImmException("Function " + st.name + " missing return statement.");
            }

            symbols.pop_back();
        },
        [&](const AST::ReturnStmt& st) -> void {
            if (st.expr) builder->CreateRet(emit_expr(*st.expr));
            else builder->CreateRetVoid();
        },[&](const AST::ForLoopStmt& st) -> void {
            llvm::Function* func = builder->GetInsertBlock()->getParent();

            llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*context, "forcond", func);
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(*context, "forloop");
            llvm::BasicBlock* inc_bb = llvm::BasicBlock::Create(*context, "forinc");
            llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context, "forcont");

            symbols.emplace_back();
            llvm::AllocaInst* iter_alloca = builder->CreateAlloca(llvm::Type::getInt32Ty(*context), nullptr, st.iter_name);
            symbols.back()[st.iter_name] = iter_alloca;

            if (auto* range = std::get_if<AST::RangeLiteralExpr>(&st.iterable->node)) {
                builder->CreateStore(emit_expr(*range->start), iter_alloca);
                builder->CreateBr(cond_bb);

                builder->SetInsertPoint(cond_bb);
                llvm::Value* curr_val = builder->CreateLoad(llvm::Type::getInt32Ty(*context), iter_alloca, "iterld");
                llvm::Value* cond = builder->CreateICmpSLT(curr_val, emit_expr(*range->end), "forcond_cmp");
                builder->CreateCondBr(cond, loop_bb, merge_bb);

                func->insert(func->end(), loop_bb); // FIX: Link the block before using it
                builder->SetInsertPoint(loop_bb);
                continue_stack.push_back(inc_bb);
                break_stack.push_back(merge_bb);

                emit_stmt(*st.body);
                if (!builder->GetInsertBlock()->getTerminator()) builder->CreateBr(inc_bb);

                func->insert(func->end(), inc_bb);
                builder->SetInsertPoint(inc_bb);
                llvm::Value* step = builder->CreateAdd(builder->CreateLoad(llvm::Type::getInt32Ty(*context), iter_alloca),
                                                       llvm::ConstantInt::get(*context, llvm::APInt(32, 1, true)), "inc");
                builder->CreateStore(step, iter_alloca);
                builder->CreateBr(cond_bb);
            } else {
                llvm::AllocaInst* idx_alloca = builder->CreateAlloca(llvm::Type::getInt32Ty(*context), nullptr, "hidden_idx");
                builder->CreateStore(llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true)), idx_alloca);

                llvm::Value* arr_ptr = emit_expr(*st.iterable);
                if (!st.iterable->inferred_type->size.has_value()) throw LLVMImmException("Dynamic array length in for loops not yet supported.");

                llvm::Value* len_val = llvm::ConstantInt::get(*context, llvm::APInt(32, *st.iterable->inferred_type->size, true));

                builder->CreateBr(cond_bb);
                builder->SetInsertPoint(cond_bb);
                llvm::Value* curr_idx = builder->CreateLoad(llvm::Type::getInt32Ty(*context), idx_alloca, "idxld");
                llvm::Value* cond = builder->CreateICmpSLT(curr_idx, len_val, "forcond_cmp");
                builder->CreateCondBr(cond, loop_bb, merge_bb);

                func->insert(func->end(), loop_bb); // FIX: Link the block before using it
                builder->SetInsertPoint(loop_bb);
                llvm::Type* arr_type = get_llvm_type(*st.iterable->inferred_type);
                llvm::Value* zero = llvm::ConstantInt::get(*context, llvm::APInt(32, 0, true));
                llvm::Value* gep = builder->CreateInBoundsGEP(arr_type, arr_ptr, {zero, curr_idx}, "forgep");
                builder->CreateStore(builder->CreateLoad(get_llvm_type({st.iterable->inferred_type->type, false, std::nullopt}), gep, "elemld"), iter_alloca);

                continue_stack.push_back(inc_bb);
                break_stack.push_back(merge_bb);

                emit_stmt(*st.body);
                if (!builder->GetInsertBlock()->getTerminator()) builder->CreateBr(inc_bb);

                func->insert(func->end(), inc_bb);
                builder->SetInsertPoint(inc_bb);
                llvm::Value* step = builder->CreateAdd(builder->CreateLoad(llvm::Type::getInt32Ty(*context), idx_alloca),
                                                       llvm::ConstantInt::get(*context, llvm::APInt(32, 1, true)), "incidx");
                builder->CreateStore(step, idx_alloca);
                builder->CreateBr(cond_bb);
            }

            continue_stack.pop_back();
            break_stack.pop_back();

            func->insert(func->end(), merge_bb);
            builder->SetInsertPoint(merge_bb);
            symbols.pop_back();
        },
        [&](const AST::WhileLoopStmt& st) -> void {
            llvm::Function* func = builder->GetInsertBlock()->getParent();
            llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*context, "whilecond", func);
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(*context, "whileloop");
            llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context, "whilecont");

            builder->CreateBr(cond_bb);
            builder->SetInsertPoint(cond_bb);
            builder->CreateCondBr(emit_expr(*st.cond), loop_bb, merge_bb);

            func->insert(func->end(), loop_bb);
            builder->SetInsertPoint(loop_bb);
            continue_stack.push_back(cond_bb);
            break_stack.push_back(merge_bb);

            emit_stmt(*st.body);
            if (!builder->GetInsertBlock()->getTerminator()) builder->CreateBr(cond_bb);

            continue_stack.pop_back();
            break_stack.pop_back();

            func->insert(func->end(), merge_bb);
            builder->SetInsertPoint(merge_bb);
        },
        [&](const AST::BreakStmt&) -> void {
            if (break_stack.empty()) throw LLVMImmException("Break statement found outside of a loop block.");
            builder->CreateBr(break_stack.back());
        },
        [&](const AST::ContinueStmt&) -> void {
            if (continue_stack.empty()) throw LLVMImmException("Continue statement found outside of a loop block.");
            builder->CreateBr(continue_stack.back());
        },
        [&](const AST::ExitStmt& st) -> void {
            llvm::Function* exit_fn = module->getFunction("exit");
            if (!exit_fn) {
                llvm::FunctionType* eft = llvm::FunctionType::get(llvm::Type::getVoidTy(*context), {llvm::Type::getInt32Ty(*context)}, false);
                exit_fn = llvm::Function::Create(eft, llvm::Function::ExternalLinkage, "exit", module.get());
            }
            builder->CreateCall(exit_fn, {emit_expr(*st.expr)});
            builder->CreateUnreachable(); // optimize dead code after exit()
        }
    }, stmt.node);
}

// executing with JIT

void LLVMImmediate::execute() {
    if (is_error) {
        std::cerr << "Cannot execute due to prior errors.\n";
        return;
    }

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::string err_str;
    llvm::ExecutionEngine* ee = llvm::EngineBuilder(std::move(module))
        .setErrorStr(&err_str)
        .setEngineKind(llvm::EngineKind::JIT)
        .create();

    if (!ee) {
        std::cerr << "Failed to construct ExecutionEngine: " << err_str << "\n";
        return;
    }

    llvm::Function* main_fn = ee->FindFunctionNamed("main");
    if (!main_fn) {
        std::cerr << "Could not find main function to execute.\n";
        return;
    }

    ee->finalizeObject();
    std::vector<llvm::GenericValue> noargs;
    llvm::GenericValue v = ee->runFunction(main_fn, noargs);

    delete ee;
}
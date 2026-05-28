//
// Created by accord.
//

#ifndef VEXC_VEXIR_HPP
#define VEXC_VEXIR_HPP

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "ast/Ast.hpp"

namespace VexIR {
    struct IRType {
        AST::VarTypeOpts base;
        bool is_address = false;
        bool is_vector = false;
        int lanes = 1;
    };

    inline std::string print_ir_type(const IRType& type) {
        std::string base = (type.base == AST::VarTypeOpts::INT32) ? "i32"
            : (type.base == AST::VarTypeOpts::BOOL) ? "bool"
            : "string_literal";
        if (type.is_address) base += "*";
        if (type.is_vector) base = "<" + std::to_string(type.lanes) + " x " + base + ">";
        return base;
    }

    inline IRType scalar_type_from_ast(const AST::VarTypeSpec& type) {
        return IRType{type.type, type.is_array, false, 1};
    }

    struct Temporary {
        std::string name;
        IRType type;
    };

    struct Storage {
        std::string name;
        AST::VarTypeSpec type;
    };

    using AddressOperand = std::variant<Storage, Temporary>;

    inline std::string print_temp(const Temporary& temp) {
        return temp.name;
    }

    inline std::string print_storage(const Storage& storage) {
        return storage.name;
    }

    inline std::string print_address_operand(const AddressOperand& operand) {
        return std::visit([](const auto& value) { return value.name; }, operand);
    }

    enum class UnaryOp { NEG, NOT, MOV };
    enum class BinaryOp { ADD, SUB, MUL, DIV, REM, AND, OR, GT, GEQ, LT, LEQ, EQ, NEQ };
    enum class VectorBinaryOp { VADD, VSUB, VMUL, VDIV, VREM, VAND, VOR };

    inline std::string print_unary_op(const UnaryOp op) {
        switch (op) {
            case UnaryOp::NEG: return "NEG";
            case UnaryOp::NOT: return "NOT";
            case UnaryOp::MOV: return "MOV";
        }
        return "INVALID_UNARY";
    }

    inline std::string print_binary_op(const BinaryOp op) {
        switch (op) {
            case BinaryOp::ADD: return "ADD";
            case BinaryOp::SUB: return "SUB";
            case BinaryOp::MUL: return "MUL";
            case BinaryOp::DIV: return "DIV";
            case BinaryOp::REM: return "REM";
            case BinaryOp::AND: return "AND";
            case BinaryOp::OR: return "OR";
            case BinaryOp::GT: return "GT";
            case BinaryOp::GEQ: return "GEQ";
            case BinaryOp::LT: return "LT";
            case BinaryOp::LEQ: return "LEQ";
            case BinaryOp::EQ: return "EQ";
            case BinaryOp::NEQ: return "NEQ";
        }
        return "INVALID_BINARY";
    }

    inline std::string print_vector_binary_op(const VectorBinaryOp op) {
        switch (op) {
            case VectorBinaryOp::VADD: return "VADD";
            case VectorBinaryOp::VSUB: return "VSUB";
            case VectorBinaryOp::VMUL: return "VMUL";
            case VectorBinaryOp::VDIV: return "VDIV";
            case VectorBinaryOp::VREM: return "VREM";
            case VectorBinaryOp::VAND: return "VAND";
            case VectorBinaryOp::VOR: return "VOR";
        }
        return "INVALID_VBINARY";
    }

    struct ConstI32Inst {
        Temporary dest;
        int value;
    };

    struct ConstBoolInst {
        Temporary dest;
        bool value;
    };

    struct ConstStrInst {
        Temporary dest;
        std::string value;
    };

    struct AllocLocalInst {
        Storage dest;
    };

    struct AllocArrayInst {
        Storage dest;
        int size;
    };

    struct AddrOfInst {
        Temporary dest;
        Storage source;
    };

    struct StoreInst {
        AddressOperand address;
        Temporary value;
    };

    struct LoadInst {
        Temporary dest;
        AddressOperand address;
    };

    struct ElemAddrInst {
        Temporary dest;
        AddressOperand base;
        Temporary index;
    };

    struct UnaryInst {
        UnaryOp op;
        Temporary dest;
        Temporary src;
    };

    struct BinaryInst {
        BinaryOp op;
        Temporary dest;
        Temporary lhs;
        Temporary rhs;
    };

    struct CallInst {
        std::optional<Temporary> dest;
        std::string function_name;
        std::vector<Temporary> args;
    };

    struct PrintInst {
        bool newline;
        std::vector<Temporary> args;
    };

    struct ReadInst {
        std::vector<Storage> targets;
    };

    struct Pack2Inst {
        Temporary dest;
        Temporary lhs;
        Temporary rhs;
    };

    struct VectorBinaryInst {
        VectorBinaryOp op;
        Temporary dest;
        Temporary lhs;
        Temporary rhs;
    };

    struct ExtractInst {
        Temporary dest;
        Temporary vec;
        int lane;
    };

    using Instruction = std::variant<
        ConstI32Inst, ConstBoolInst, ConstStrInst, AllocLocalInst, AllocArrayInst, AddrOfInst,
        StoreInst, LoadInst, ElemAddrInst, UnaryInst, BinaryInst, CallInst, PrintInst, ReadInst,
        Pack2Inst, VectorBinaryInst, ExtractInst
    >;

    struct IncomingEdge {
        std::string block_name;
        Temporary value;
    };

    struct PhiInst {
        Temporary dest;
        std::vector<IncomingEdge> incomings;
    };

    struct JumpTerminator {
        std::string target;
    };

    struct BranchTerminator {
        Temporary condition;
        std::string true_target;
        std::string false_target;
    };

    struct ReturnTerminator {
        std::optional<Temporary> value;
    };

    struct ExitTerminator {
        Temporary code;
    };

    using Terminator = std::variant<JumpTerminator, BranchTerminator, ReturnTerminator, ExitTerminator>;

    struct BasicBlock {
        std::string name;
        std::vector<PhiInst> phis;
        std::vector<Instruction> instructions;
        std::optional<Terminator> terminator;
    };

    struct Param {
        std::string name;
        IRType type;
    };

    struct Function {
        std::string name;
        std::vector<Param> params;
        std::optional<IRType> return_type;
        std::vector<BasicBlock> blocks;
        bool is_entrypoint = false;
    };

    struct Module {
        std::vector<Function> functions;
    };

    inline VectorBinaryOp vector_op_from_binary(const BinaryOp op) {
        switch (op) {
            case BinaryOp::ADD: return VectorBinaryOp::VADD;
            case BinaryOp::SUB: return VectorBinaryOp::VSUB;
            case BinaryOp::MUL: return VectorBinaryOp::VMUL;
            case BinaryOp::DIV: return VectorBinaryOp::VDIV;
            case BinaryOp::REM: return VectorBinaryOp::VREM;
            case BinaryOp::AND: return VectorBinaryOp::VAND;
            case BinaryOp::OR: return VectorBinaryOp::VOR;
            default: break;
        }
        throw std::runtime_error("VexIR internal error: Binary op " + print_binary_op(op) + " is not vectorizable.");
    }

    inline bool is_vectorizable_binary(const BinaryOp op) {
        switch (op) {
            case BinaryOp::ADD:
            case BinaryOp::SUB:
            case BinaryOp::MUL:
            case BinaryOp::DIV:
            case BinaryOp::REM:
            case BinaryOp::AND:
            case BinaryOp::OR:
                return true;
            default:
                return false;
        }
    }

    inline std::string print_instruction(const Instruction& instruction) {
        return std::visit(overloaded{
            [](const ConstI32Inst& inst) {
                return "CONST_I32 " + print_temp(inst.dest) + " (" + std::to_string(inst.value) + ")";
            },
            [](const ConstBoolInst& inst) {
                return "CONST_BOOL " + print_temp(inst.dest) + " (" + std::string(inst.value ? "true" : "false") + ")";
            },
            [](const ConstStrInst& inst) {
                return "CONST_STR " + print_temp(inst.dest) + " (\"" + inst.value + "\")";
            },
            [](const AllocLocalInst& inst) {
                return "ALLOC_LOCAL " + print_storage(inst.dest);
            },
            [](const AllocArrayInst& inst) {
                return "ALLOC_ARRAY " + print_storage(inst.dest) + " " + std::to_string(inst.size);
            },
            [](const AddrOfInst& inst) {
                return "ADDR_OF " + print_temp(inst.dest) + " " + print_storage(inst.source);
            },
            [](const StoreInst& inst) {
                return "STORE " + print_address_operand(inst.address) + " " + print_temp(inst.value);
            },
            [](const LoadInst& inst) {
                return "LOAD " + print_temp(inst.dest) + " " + print_address_operand(inst.address);
            },
            [](const ElemAddrInst& inst) {
                return "ELEM_ADDR " + print_temp(inst.dest) + " " + print_address_operand(inst.base) + " " + print_temp(inst.index);
            },
            [](const UnaryInst& inst) {
                return print_unary_op(inst.op) + " " + print_temp(inst.dest) + " " + print_temp(inst.src);
            },
            [](const BinaryInst& inst) {
                return print_binary_op(inst.op) + " " + print_temp(inst.dest) + " " + print_temp(inst.lhs) + " " + print_temp(inst.rhs);
            },
            [](const CallInst& inst) {
                std::string resp;
                if (inst.dest) resp = "CALL " + print_temp(*inst.dest) + " (" + inst.function_name + ")";
                else resp = "CALL_VOID (" + inst.function_name + ")";
                for (const auto& arg : inst.args) resp += " " + print_temp(arg);
                return resp;
            },
            [](const PrintInst& inst) {
                std::string resp = inst.newline ? "PRINTLN" : "PRINT";
                for (const auto& arg : inst.args) resp += " " + print_temp(arg);
                return resp;
            },
            [](const ReadInst& inst) {
                std::string resp = "READ";
                for (const auto& target : inst.targets) resp += " " + print_storage(target);
                return resp;
            },
            [](const Pack2Inst& inst) {
                return "PACK2 " + print_temp(inst.dest) + " " + print_temp(inst.lhs) + " " + print_temp(inst.rhs);
            },
            [](const VectorBinaryInst& inst) {
                return print_vector_binary_op(inst.op) + " " + print_temp(inst.dest) + " " + print_temp(inst.lhs) + " " + print_temp(inst.rhs);
            },
            [](const ExtractInst& inst) {
                return "EXTRACT " + print_temp(inst.dest) + " " + print_temp(inst.vec) + " " + std::to_string(inst.lane);
            }
        }, instruction);
    }

    inline std::string print_phi(const PhiInst& phi) {
        std::string resp = "PHI " + print_temp(phi.dest);
        for (const auto& incoming : phi.incomings) {
            resp += " (" + incoming.block_name + " " + print_temp(incoming.value) + ")";
        }
        return resp;
    }

    inline std::string print_terminator(const Terminator& terminator) {
        return std::visit(overloaded{
            [](const JumpTerminator& term) {
                return "JUMP " + term.target;
            },
            [](const BranchTerminator& term) {
                return "BRANCH " + print_temp(term.condition) + " " + term.true_target + " " + term.false_target;
            },
            [](const ReturnTerminator& term) {
                return term.value ? "RETURN " + print_temp(*term.value) : "RETURN_VOID";
            },
            [](const ExitTerminator& term) {
                return "EXIT " + print_temp(term.code);
            }
        }, terminator);
    }

    inline std::string print_function(const Function& function) {
        std::string resp = "FUNCTION (" + function.name + ")\n";
        for (const auto& param : function.params) resp += "PARAM " + param.name + "\n";
        for (const auto& block : function.blocks) {
            resp += "BLOCK " + block.name + "\n";
            for (const auto& phi : block.phis) resp += "    " + print_phi(phi) + "\n";
            for (const auto& instruction : block.instructions) resp += "    " + print_instruction(instruction) + "\n";
            if (block.terminator) resp += "    " + print_terminator(*block.terminator) + "\n";
        }
        resp += "FUNC_END";
        return resp;
    }

    inline std::string print_module(const Module& module) {
        std::string resp;
        for (size_t i = 0; i < module.functions.size(); ++i) {
            if (i) resp += "\n\n";
            resp += print_function(module.functions[i]);
        }
        return resp;
    }
}

#endif //VEXC_VEXIR_HPP

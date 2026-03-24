// TypeMapper.hpp - 类型映射
// 将 Paani 类型映射到 C 类型

#pragma once
#include "paani_ast.hpp"
#include "Naming.hpp"
#include <string>
#include <unordered_set>

namespace paani {

class TypeMapper {
public:
    // Handle types registered from package
    std::unordered_set<std::string> handleTypes;
    std::string packageName;

    // Map Paani type to C type
    std::string map(const Type& type) const {
        switch (type.kind) {
            case TypeKind::Void: return "void";
            case TypeKind::Bool: return "int";
            case TypeKind::I8: return "int8_t";
            case TypeKind::I16: return "int16_t";
            case TypeKind::I32: return "int32_t";
            case TypeKind::I64: return "int64_t";
            case TypeKind::U8: return "uint8_t";
            case TypeKind::U16: return "uint16_t";
            case TypeKind::U32: return "uint32_t";
            case TypeKind::U64: return "uint64_t";
            case TypeKind::F32: return "float";
            case TypeKind::F64: return "double";
            case TypeKind::String: return "const char*";
            case TypeKind::Entity: return "paani_entity_t";
            case TypeKind::Handle: return "void*";
            case TypeKind::Pointer: return map(*type.base) + "*";
            case TypeKind::Array: {
                // Array type will be handled specially in field declaration
                // For now return the base type, array size will be appended in field declaration
                return map(*type.base);
            }
            case TypeKind::UserDefined: {
                // Check if it's a handle type
                if (handleTypes.count(type.name)) {
                    return "void*";
                }
                // Check if already prefixed
                if (type.name.find("__") != std::string::npos) {
                    return type.name;
                }
                return Naming::withPackage(packageName, type.name);
            }
            case TypeKind::ExternType: return Naming::externType(packageName, type.name);
            case TypeKind::Function: return "void(*)()";
            default: return "int32_t";
        }
    }
    
    // Check if type is a handle
    bool isHandle(const Type& type) const {
        if (type.kind == TypeKind::Handle) return true;
        if (type.kind == TypeKind::UserDefined && handleTypes.count(type.name)) {
            return true;
        }
        return false;
    }
    
    std::string mapBinOp(BinOp op) const {
        switch (op) {
            case BinOp::Add: return "+";
            case BinOp::Sub: return "-";
            case BinOp::Mul: return "*";
            case BinOp::Div: return "/";
            case BinOp::Mod: return "%";
            case BinOp::Eq: return "==";
            case BinOp::Ne: return "!=";
            case BinOp::Lt: return "<";
            case BinOp::Gt: return ">";
            case BinOp::Le: return "<=";
            case BinOp::Ge: return ">=";
            case BinOp::And: return "&&";
            case BinOp::Or: return "||";
            case BinOp::Assign: return "=";
            case BinOp::AddAssign: return "+=";
            case BinOp::SubAssign: return "-=";
            // Bitwise operators
            case BinOp::BitAnd: return "&";
            case BinOp::BitOr: return "|";
            case BinOp::BitXor: return "^";
            case BinOp::BitShl: return "<<";
            case BinOp::BitShr: return ">>";
            default: return "+";
        }
    }
    
    std::string mapUnOp(UnOp op) const {
        switch (op) {
            case UnOp::Neg: return "-";
            case UnOp::Not: return "!";
            case UnOp::BitNot: return "~";
            default: return "+";
        }
    }
};

} // namespace paani

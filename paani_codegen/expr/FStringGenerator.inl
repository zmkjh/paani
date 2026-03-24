#pragma once

#include "FStringGenerator.hpp"
#include <cstdarg>

namespace paani {

inline FStringGenerator::FStringGenerator(CodeGenContext& ctx, int& tempCounter, 
                                                         TypeLookupFn typeLookup,
                                                         ComponentLookupFn componentLookup,
                                                         FieldTypeLookupFn fieldTypeLookup,
                                                         ExprTypeInferenceFn exprTypeInference)
    : ctx_(ctx)
    , tempCounter_(tempCounter)
    , typeLookup_(typeLookup)
    , componentLookup_(componentLookup)
    , fieldTypeLookup_(fieldTypeLookup)
    , exprTypeInference_(exprTypeInference) {}

inline std::string FStringGenerator::generate(const FStringExpr& expr) {
    if (expr.parts.empty()) {
        return "\"\"";
    }
    
    // Simple case: single literal part
    if (expr.parts.size() == 1 && !expr.parts[0].isExpr) {
        return "\"" + expr.parts[0].text + "\"";
    }
    
    // Complex case: need to build string
    std::string tempVar = getTempVarName();
    
    // Allocate buffer
    ctx_.writeImplLn("char " + tempVar + "[256];");
    ctx_.writeImplLn(tempVar + "[0] = '\\0';");
    
    for (const auto& part : expr.parts) {
        if (!part.isExpr) {
            // Append literal text
            ctx_.writeImplLn("strcat(" + tempVar + ", \"" + part.text + "\");");
        } else if (part.expr) {
            // Append expression
            std::string formatSpec = getFormatSpecifier(*part.expr);
            std::string exprCode = generateExpression(*part.expr);
            
            ctx_.writeImplLn("{");
            ctx_.pushIndent();
            ctx_.writeImplLn("char __buf[64];");
            ctx_.writeImplLn("snprintf(__buf, sizeof(__buf), \"" + formatSpec + "\", " + exprCode + ");");
            ctx_.writeImplLn("strcat(" + tempVar + ", __buf);");
            ctx_.popIndent();
            ctx_.writeImplLn("}");
        }
    }
    
    return tempVar;
}

inline std::string FStringGenerator::generateExpression(const Expr& expr) {
    // Handle various expression types
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        return std::string(id->name);
    }
    if (auto* intExpr = dynamic_cast<const IntegerExpr*>(&expr)) {
        return std::to_string(intExpr->value);
    }
    if (auto* floatExpr = dynamic_cast<const FloatExpr*>(&expr)) {
        return std::to_string(floatExpr->value) + "f";
    }
    if (auto* boolExpr = dynamic_cast<const BoolExpr*>(&expr)) {
        return boolExpr->value ? "1" : "0";
    }
    if (auto* strExpr = dynamic_cast<const StringExpr*>(&expr)) {
        return "\"" + strExpr->value + "\"";
    }
    
    // Binary expressions
    if (auto* binExpr = dynamic_cast<const BinaryExpr*>(&expr)) {
        std::string left = generateExpression(*binExpr->left);
        std::string right = generateExpression(*binExpr->right);
        std::string op = getBinOpString(binExpr->op);
        return "(" + left + " " + op + " " + right + ")";
    }
    
    // Unary expressions
    if (auto* unExpr = dynamic_cast<const UnaryExpr*>(&expr)) {
        std::string operand = generateExpression(*unExpr->operand);
        std::string op = getUnOpString(unExpr->op);
        return "(" + op + operand + ")";
    }
    
    // Member access (e.g., entity.Position.x)
    if (auto* memberExpr = dynamic_cast<const MemberExpr*>(&expr)) {
        // Check if this is a component access in trait system (entity.Component.field)
        if (componentLookup_) {
            // Try to match entity.Component pattern
            if (auto* innerMember = dynamic_cast<const MemberExpr*>(memberExpr->object.get())) {
                if (auto* entityId = dynamic_cast<const IdentifierExpr*>(innerMember->object.get())) {
                    std::string entityName = entityId->name;
                    std::string compName = innerMember->member;
                    std::string fieldName = memberExpr->member;
                    
                    // Look up component pointer: entity.Component -> _entity_Component
                    std::string pointerName = componentLookup_(entityName + "." + compName);
                    if (!pointerName.empty()) {
                        // Found! Use pointer access: _entity_Component->field
                        return pointerName + "->" + fieldName;
                    }
                }
            }
        }
        
        // Default: simple member access
        std::string object = generateExpression(*memberExpr->object);
        return object + "." + memberExpr->member;
    }
    
    // Index access (e.g., arr[i])
    if (auto* indexExpr = dynamic_cast<const IndexExpr*>(&expr)) {
        std::string object = generateExpression(*indexExpr->object);
        std::string index = generateExpression(*indexExpr->index);
        return object + "[" + index + "]";
    }
    
    // Function calls
    if (auto* callExpr = dynamic_cast<const CallExpr*>(&expr)) {
        std::string callee = generateExpression(*callExpr->callee);
        std::string result = callee + "(";
        for (size_t i = 0; i < callExpr->args.size(); i++) {
            if (i > 0) result += ", ";
            result += generateExpression(*callExpr->args[i]);
        }
        result += ")";
        return result;
    }
    
    // Spawn expression
    if (auto* spawnExpr = dynamic_cast<const SpawnExpr*>(&expr)) {
        return "paani_entity_create(__paani_gen_world)";
    }
    
    return "/* unsupported expression */";
}

inline std::string FStringGenerator::getBinOpString(BinOp op) {
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
        case BinOp::BitAnd: return "&";
        case BinOp::BitOr: return "|";
        case BinOp::BitXor: return "^";
        case BinOp::BitShl: return "<<";
        case BinOp::BitShr: return ">>";
        default: return "+";
    }
}

inline std::string FStringGenerator::getUnOpString(UnOp op) {
    switch (op) {
        case UnOp::Neg: return "-";
        case UnOp::Not: return "!";
        case UnOp::BitNot: return "~";
        case UnOp::Deref: return "*";
        case UnOp::AddrOf: return "&";
        default: return "+";
    }
}

inline std::string FStringGenerator::formatSpecifierFromType(const std::string& type) {
    if (type == "float" || type == "double") return "%f";
    if (type == "int32_t" || type == "int" || type == "uint32_t") return "%d";
    if (type == "int64_t" || type == "uint64_t") return "%lld";
    if (type == "const char*" || type.find("*") != std::string::npos) return "%s";
    return "%d"; // default
}

inline std::string FStringGenerator::getFormatSpecifier(const Expr& expr) {
    // For literal expressions, use their known types (highest priority)
    if (dynamic_cast<const FloatExpr*>(&expr)) return "%f";
    if (dynamic_cast<const IntegerExpr*>(&expr)) return "%d";
    if (dynamic_cast<const StringExpr*>(&expr)) return "%s";
    if (dynamic_cast<const BoolExpr*>(&expr)) return "%d";
    
    // For identifier expressions, look up in symbol table
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        if (typeLookup_) {
            std::string type = typeLookup_(id->name);
            if (!type.empty()) {
                return formatSpecifierFromType(type);
            }
        }
    }
    
    // For member expressions (e.g., entity.Component.field), try field type lookup first
    if (auto* memberExpr = dynamic_cast<const MemberExpr*>(&expr)) {
        std::string fieldName = memberExpr->member;
        
        // Pattern: entity.Component.field
        if (auto* innerMember = dynamic_cast<const MemberExpr*>(memberExpr->object.get())) {
            std::string compName = innerMember->member;
            
            if (fieldTypeLookup_) {
                std::string fieldType = fieldTypeLookup_(compName + "." + fieldName);
                if (!fieldType.empty()) {
                    return formatSpecifierFromType(fieldType);
                }
            }
        }
    }
    
    // Try expression type inference as fallback
    if (exprTypeInference_) {
        std::string exprType = exprTypeInference_(expr);
        if (!exprType.empty() && exprType != "int32_t") {  // Avoid default int32_t
            return formatSpecifierFromType(exprType);
        }
    }
    
    // For member expressions (e.g., entity.Component.field) - second attempt with pointer pattern
    if (auto* memberExpr = dynamic_cast<const MemberExpr*>(&expr)) {
        std::string fieldName = memberExpr->member;
        
        // Check if this is a component field access in trait system
        // Pattern 1: entity.Component.field (before conversion)
        if (auto* innerMember = dynamic_cast<const MemberExpr*>(memberExpr->object.get())) {
            if (auto* entityId = dynamic_cast<const IdentifierExpr*>(innerMember->object.get())) {
                std::string compName = innerMember->member;
                
                // Try to look up field type from symbol table: Position.x
                if (fieldTypeLookup_) {
                    std::string fieldType = fieldTypeLookup_(compName + "." + fieldName);
                    if (!fieldType.empty()) {
                        return formatSpecifierFromType(fieldType);
                    }
                }
            }
        }
        
        // Pattern 2: _e_Component->field (after componentLookup conversion)
        // The object is now a pointer variable like _e_Position
        if (auto* id = dynamic_cast<const IdentifierExpr*>(memberExpr->object.get())) {
            std::string ptrName = id->name;
            
            // Extract component name from _e_Position pattern
            // Pattern: _{entity}_{Component} -> extract Component
            if (ptrName.length() > 2 && ptrName[0] == '_') {
                size_t firstUnderscore = ptrName.find('_', 1);
                if (firstUnderscore != std::string::npos) {
                    std::string compName = ptrName.substr(firstUnderscore + 1);
                    
                    if (fieldTypeLookup_) {
                        std::string fieldType = fieldTypeLookup_(compName + "." + fieldName);
                        if (!fieldType.empty()) {
                            return formatSpecifierFromType(fieldType);
                        }
                    }
                }
            }
        }
    }
    
    // For binary expressions, try to infer from operands
    if (auto* binExpr = dynamic_cast<const BinaryExpr*>(&expr)) {
        // Arithmetic operations: use operand type
        if (binExpr->op == BinOp::Add || binExpr->op == BinOp::Sub || 
            binExpr->op == BinOp::Mul || binExpr->op == BinOp::Div || binExpr->op == BinOp::Mod) {
            // Try left operand first
            std::string leftSpec = getFormatSpecifier(*binExpr->left);
            if (leftSpec != "%d" || dynamic_cast<const IntegerExpr*>(binExpr->left.get())) {
                return leftSpec;
            }
            // Try right operand
            std::string rightSpec = getFormatSpecifier(*binExpr->right);
            if (rightSpec != "%d" || dynamic_cast<const IntegerExpr*>(binExpr->right.get())) {
                return rightSpec;
            }
            return "%d";
        }
        // Comparison/logical operations produce booleans (int)
        return "%d";
    }
    
    // For unary expressions
    if (auto* unExpr = dynamic_cast<const UnaryExpr*>(&expr)) {
        if (unExpr->op == UnOp::Neg) {
            return getFormatSpecifier(*unExpr->operand);
        }
        if (unExpr->op == UnOp::Not || unExpr->op == UnOp::BitNot) {
            return "%d";
        }
    }
    
    return "%d"; // default
}

inline std::string FStringGenerator::getTempVarName() {
    return "__fstr_" + std::to_string(tempCounter_++);
}

} // namespace paani


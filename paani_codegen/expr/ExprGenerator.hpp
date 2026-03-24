// ExprGenerator.hpp - 表达式生成基类

#pragma once
#include "paani_ast.hpp"
#include "../core/CodeGenContext.hpp"
#include "../core/TypeMapper.hpp"
#include "../handle/HandleTracker.hpp"
#include <string>
#include <memory>

namespace paani {

class ExprGenerator {
protected:
    CodeGenContext& ctx_;
    const TypeMapper& typeMapper_;
    HandleTracker& handleTracker_;
    
    // Function info for type inference
    const std::unordered_map<std::string, TypeKind>& funcReturnTypes_;
    const std::unordered_map<std::string, std::string>& funcReturnTypeNames_;
    
    // Variable types for identifier lookup
    std::unordered_map<std::string, std::string>& varTypes_;
    
public:
    ExprGenerator(CodeGenContext& ctx, const TypeMapper& typeMapper,
                  HandleTracker& handleTracker,
                  const std::unordered_map<std::string, TypeKind>& funcReturnTypes,
                  const std::unordered_map<std::string, std::string>& funcReturnTypeNames,
                  std::unordered_map<std::string, std::string>& varTypes)
        : ctx_(ctx), typeMapper_(typeMapper), handleTracker_(handleTracker),
          funcReturnTypes_(funcReturnTypes), funcReturnTypeNames_(funcReturnTypeNames),
          varTypes_(varTypes) {}
    
    virtual ~ExprGenerator() = default;
    
    // Main entry point
    virtual std::string generate(const Expr& expr) {
        return genExpr(expr);
    }
    
protected:
    // Dispatch to specific generators
    std::string genExpr(const Expr& expr) {
        if (auto* e = dynamic_cast<const IntegerExpr*>(&expr)) return genInteger(*e);
        if (auto* e = dynamic_cast<const FloatExpr*>(&expr)) return genFloat(*e);
        if (auto* e = dynamic_cast<const BoolExpr*>(&expr)) return genBool(*e);
        if (auto* e = dynamic_cast<const StringExpr*>(&expr)) return genString(*e);
        if (auto* e = dynamic_cast<const FStringExpr*>(&expr)) return genFString(*e);
        if (auto* e = dynamic_cast<const IdentifierExpr*>(&expr)) return genIdentifier(*e);
        if (auto* e = dynamic_cast<const BinaryExpr*>(&expr)) return genBinary(*e);
        if (auto* e = dynamic_cast<const UnaryExpr*>(&expr)) return genUnary(*e);
        if (auto* e = dynamic_cast<const CallExpr*>(&expr)) return genCall(*e);
        if (auto* e = dynamic_cast<const MemberExpr*>(&expr)) return genMember(*e);
        if (auto* e = dynamic_cast<const IndexExpr*>(&expr)) return genIndex(*e);
        if (auto* e = dynamic_cast<const HasExpr*>(&expr)) return genHas(*e);
        if (auto* e = dynamic_cast<const SpawnExpr*>(&expr)) return genSpawn(*e);
        return "0";
    }
    
    // Basic generators
    std::string genInteger(const IntegerExpr& expr) { return std::string(expr.value); }
    std::string genFloat(const FloatExpr& expr) { return std::string(expr.value); }
    std::string genBool(const BoolExpr& expr) { return expr.value ? "1" : "0"; }
    std::string genString(const StringExpr& expr) { 
        return "\"" + std::string(expr.value) + "\""; 
    }
    std::string genIdentifier(const IdentifierExpr& expr) { 
        return std::string(expr.name); 
    }
    
    // To be implemented by subclasses or specialized generators
    virtual std::string genFString(const FStringExpr& expr);
    virtual std::string genBinary(const BinaryExpr& expr);
    virtual std::string genUnary(const UnaryExpr& expr);
    virtual std::string genCall(const CallExpr& expr);
    virtual std::string genMember(const MemberExpr& expr);
    virtual std::string genIndex(const IndexExpr& expr);
    virtual std::string genHas(const HasExpr& expr);
    virtual std::string genSpawn(const SpawnExpr& expr);
    
    // Type inference
    std::string inferType(const Expr& expr) const;
    bool isHandleType(const Expr& expr) const;
};

} // namespace paani

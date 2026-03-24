// VarDeclGenerator.hpp - 变量声明生成
// 处理 handle 类型检测和移动语义

#pragma once
#include "paani_ast.hpp"
#include "../core/CodeGenContext.hpp"
#include "../core/TypeMapper.hpp"
#include "../core/Naming.hpp"
#include "../handle/HandleTracker.hpp"
#include "../handle/HandleEmitter.hpp"
#include "../expr/ExprGenerator.hpp"
#include <memory>

namespace paani {

class VarDeclGenerator {
private:
    CodeGenContext& ctx_;
    const TypeMapper& typeMapper_;
    HandleTracker& handleTracker_;
    HandleEmitter handleEmitter_;
    ExprGenerator& exprGen_;
    
    // Handle destructors: typeName -> dtorFunc
    const std::unordered_map<std::string, std::string>& handleDtors_;
    
    // Variable types for inference
    std::unordered_map<std::string, std::string>& varTypes_;
    
    // Function return type names for handle detection
    const std::unordered_map<std::string, std::string>& funcReturnTypeNames_;
    
    std::string packageName_;
    
public:
    VarDeclGenerator(CodeGenContext& ctx, const TypeMapper& typeMapper,
                     HandleTracker& handleTracker, ExprGenerator& exprGen,
                     const std::unordered_map<std::string, std::string>& handleDtors,
                     std::unordered_map<std::string, std::string>& varTypes,
                     const std::unordered_map<std::string, std::string>& funcReturnTypeNames,
                     const std::string& packageName)
        : ctx_(ctx), typeMapper_(typeMapper), handleTracker_(handleTracker),
          handleEmitter_(handleTracker, ctx), exprGen_(exprGen),
          handleDtors_(handleDtors), varTypes_(varTypes),
          funcReturnTypeNames_(funcReturnTypeNames), packageName_(packageName) {}
    
    void generate(const VarDeclStmt& stmt) {
        std::string type;
        std::string handleTypeName;
        
        // Determine type
        if (stmt.type) {
            type = determineTypeWithHandle(*stmt.type, handleTypeName);
        } else if (stmt.init) {
            type = exprGen_.inferType(*stmt.init);
            handleTypeName = inferHandleTypeFromInit(*stmt.init, type);
        } else {
            type = "int32_t";
        }
        
        std::string name = stmt.name;
        varTypes_[name] = type;
        
        // Register handle variable
        if (!handleTypeName.empty() && handleTracker_.isActive()) {
            auto dtorIt = handleDtors_.find(handleTypeName);
            if (dtorIt != handleDtors_.end()) {
                handleTracker_.registerHandle(name, dtorIt->second, handleTypeName);
            }
        }
        
        // Generate declaration
        if (stmt.init && isHandleMoveInit(*stmt.init)) {
            // Handle move semantics: var = source; source = NULL;
            generateMoveDecl(name, type, *stmt.init);
        } else {
            generateSimpleDecl(name, type, stmt.init.get());
        }
    }
    
    // Register function parameter as handle
    void registerParam(const std::string& paramName, const Type& paramType) {
        if (paramType.kind == TypeKind::UserDefined && 
            typeMapper_.handleTypes.count(paramType.name)) {
            auto dtorIt = handleDtors_.find(paramType.name);
            if (dtorIt != handleDtors_.end()) {
                handleTracker_.registerHandle(paramName, dtorIt->second, paramType.name);
            }
        }
    }
    
private:
    std::string determineTypeWithHandle(const Type& type, std::string& handleTypeName) {
        if (type.kind == TypeKind::UserDefined) {
            if (typeMapper_.handleTypes.count(type.name)) {
                handleTypeName = type.name;
                return "void*";
            }
            // Data component type
            if (type.name.find("__") != std::string::npos) {
                return type.name;
            }
            return Naming::component(packageName_, type.name);
        }
        return typeMapper_.map(type);
    }
    
    std::string inferHandleTypeFromInit(const Expr& init, const std::string& inferredType) {
        if (inferredType != "void*") return "";
        
        // Check if from function call
        if (auto* call = dynamic_cast<const CallExpr*>(&init)) {
            if (auto* id = dynamic_cast<const IdentifierExpr*>(call->callee.get())) {
                auto it = funcReturnTypeNames_.find(id->name);
                if (it != funcReturnTypeNames_.end() && 
                    typeMapper_.handleTypes.count(it->second)) {
                    return it->second;
                }
            }
        }
        
        // Check if from handle variable
        if (auto* id = dynamic_cast<const IdentifierExpr*>(&init)) {
            if (handleTracker_.isHandle(id->name)) {
                auto it = handleTracker_.varTypes.find(id->name);
                if (it != handleTracker_.varTypes.end()) {
                    return it->second;
                }
            }
        }
        
        return "";
    }
    
    bool isHandleMoveInit(const Expr& init) {
        if (auto* id = dynamic_cast<const IdentifierExpr*>(&init)) {
            return handleTracker_.isHandle(id->name);
        }
        return false;
    }
    
    void generateMoveDecl(const std::string& name, const std::string& type, const Expr& init) {
        auto* id = dynamic_cast<const IdentifierExpr*>(&init);
        std::string sourceName = id->name;
        
        ctx_.writeImpl(ctx_.indent() + type + " " + name + " = " + sourceName + ";\n");
        handleEmitter_.emitMove(sourceName);
    }
    
    void generateSimpleDecl(const std::string& name, const std::string& type, const Expr* init) {
        ctx_.writeImpl(ctx_.indent() + type + " " + name);
        if (init) {
            ctx_.writeImpl(" = " + exprGen_.generate(*init));
        }
        ctx_.writeImpl(";\n");
    }
};

} // namespace paani

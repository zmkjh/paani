// CallExprGenerator.hpp - 函数调用表达式生成
// 处理参数移动语义

#pragma once
#include "ExprGenerator.hpp"
#include "../core/Naming.hpp"
#include "../handle/HandleEmitter.hpp"
#include <vector>

namespace paani {

class CallExprGenerator {
private:
    ExprGenerator& exprGen_;
    HandleTracker& handleTracker_;
    HandleEmitter handleEmitter_;
    CodeGenContext& ctx_;
    
    // Function classifications
    const std::unordered_set<std::string>& externFunctions_;
    const std::unordered_set<std::string>& exportFunctions_;
    const std::unordered_set<std::string>& internalFunctions_;
    const std::unordered_map<std::string, std::string>& packageAliases_;
    const std::unordered_set<std::string>& usedPackages_;
    std::string packageName_;
    bool isEntryPoint_;  // True if this is the entry point package
    
public:
    CallExprGenerator(ExprGenerator& exprGen, HandleTracker& handleTracker,
                      CodeGenContext& ctx,
                      const std::unordered_set<std::string>& externFunctions,
                      const std::unordered_set<std::string>& exportFunctions,
                      const std::unordered_set<std::string>& internalFunctions,
                      const std::unordered_map<std::string, std::string>& packageAliases,
                      const std::unordered_set<std::string>& usedPackages,
                      const std::string& packageName,
                      bool isEntryPoint = false)
        : exprGen_(exprGen), handleTracker_(handleTracker),
          handleEmitter_(handleTracker, ctx), ctx_(ctx),
          externFunctions_(externFunctions), exportFunctions_(exportFunctions),
          internalFunctions_(internalFunctions), packageAliases_(packageAliases),
          usedPackages_(usedPackages), packageName_(packageName),
          isEntryPoint_(isEntryPoint) {}
    
    // Generate call expression
    std::string generate(const CallExpr& expr) {
        std::string callee;
        std::string result;
        
        // Determine function type and generate appropriate call
        if (auto* idExpr = dynamic_cast<const IdentifierExpr*>(expr.callee.get())) {
            return generateDirectCall(expr, idExpr->name);
        } else if (auto* memberExpr = dynamic_cast<const MemberExpr*>(expr.callee.get())) {
            return generateQualifiedCall(expr, memberExpr);
        }
        
        // Fallback: generic call
        callee = exprGen_.generate(*expr.callee);
        result = callee + "(";
        for (size_t i = 0; i < expr.args.size(); i++) {
            if (i > 0) result += ", ";
            result += exprGen_.generate(*expr.args[i]);
        }
        result += ")";
        return result;
    }
    
    // Get handle arguments for move semantics
    std::vector<std::string> getHandleArgs(const CallExpr& expr) {
        std::vector<std::string> handleArgs;
        for (const auto& arg : expr.args) {
            if (auto* id = dynamic_cast<const IdentifierExpr*>(arg.get())) {
                if (handleTracker_.isHandle(id->name)) {
                    handleArgs.push_back(id->name);
                }
            }
        }
        return handleArgs;
    }
    
private:
    std::string generateDirectCall(const CallExpr& expr, const std::string& funcName) {
        std::string result;
        
        // Check function type
        if (externFunctions_.count(funcName)) {
            // Extern function: direct call
            result = funcName + "(";
            for (size_t i = 0; i < expr.args.size(); i++) {
                if (i > 0) result += ", ";
                result += exprGen_.generate(*expr.args[i]);
            }
            result += ")";
        } else if (exportFunctions_.count(funcName)) {
            // Export function: qualified name with world
            result = Naming::exportFunction(packageName_, funcName) + "(" + ctx_.getCurrentWorld();
            for (const auto& arg : expr.args) {
                result += ", " + exprGen_.generate(*arg);
            }
            result += ")";
        } else {
            // Internal function: qualified name with world
            result = Naming::internalFunction(packageName_, funcName) + "(" + ctx_.getCurrentWorld();
            for (const auto& arg : expr.args) {
                result += ", " + exprGen_.generate(*arg);
            }
            result += ")";
        }
        
        return result;
    }
    
    std::string generateQualifiedCall(const CallExpr& expr, const MemberExpr* memberExpr) {
        // Build path: package.module.function
        std::vector<std::string> pathParts;
        const Expr* current = expr.callee.get();
        
        while (auto* me = dynamic_cast<const MemberExpr*>(current)) {
            pathParts.push_back(me->member);
            current = me->object.get();
        }
        
        if (auto* idExpr = dynamic_cast<const IdentifierExpr*>(current)) {
            pathParts.push_back(idExpr->name);
        }
        
        std::reverse(pathParts.begin(), pathParts.end());
        
        if (pathParts.size() >= 2) {
            std::string targetPackage = pathParts[0];
            auto aliasIt = packageAliases_.find(targetPackage);
            if (aliasIt != packageAliases_.end()) {
                targetPackage = aliasIt->second;
            }
            
            std::string fullFuncName = targetPackage;
            for (size_t i = 1; i < pathParts.size(); i++) {
                fullFuncName += "__" + pathParts[i];
            }
            
            // Determine world
            // Entry point: use target package's world variable
            // Non-entry point: use current context's world (function parameter)
            std::string worldName;
            if (isEntryPoint_ && usedPackages_.count(targetPackage)) {
                worldName = "__paani_gen_" + targetPackage + "_w";
            } else {
                worldName = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
            }
            
            std::string result = fullFuncName + "(" + worldName;
            for (const auto& arg : expr.args) {
                result += ", " + exprGen_.generate(*arg);
            }
            result += ")";
            return result;
        }
        
        return "";
    }
};

} // namespace paani

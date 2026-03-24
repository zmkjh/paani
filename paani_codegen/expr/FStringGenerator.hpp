#pragma once

#include "../core/CodeGenContext.hpp"
#include "../../paani_ast.hpp"
#include <sstream>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace paani {

// FString (formatted string) generator
class FStringGenerator {
public:
    // Type lookup function: given variable name, return its C type
    using TypeLookupFn = std::function<std::string(const std::string&)>;
    // Component pointer lookup function: given entity.Component, return pointer variable name
    using ComponentLookupFn = std::function<std::string(const std::string&)>;
    // Component field type lookup: given Component.field, return C type
    using FieldTypeLookupFn = std::function<std::string(const std::string&)>;
    // Expression type inference function
    using ExprTypeInferenceFn = std::function<std::string(const Expr&)>;
    
    FStringGenerator(CodeGenContext& ctx, int& tempCounter, 
                     TypeLookupFn typeLookup = nullptr,
                     ComponentLookupFn componentLookup = nullptr,
                     FieldTypeLookupFn fieldTypeLookup = nullptr,
                     ExprTypeInferenceFn exprTypeInference = nullptr);
    
    // Generate formatted string expression
    std::string generate(const FStringExpr& expr);
    
private:
    CodeGenContext& ctx_;
    int& tempCounter_;
    TypeLookupFn typeLookup_;
    ComponentLookupFn componentLookup_;
    FieldTypeLookupFn fieldTypeLookup_;
    ExprTypeInferenceFn exprTypeInference_;
    
    // Code generation helpers
    std::string generateExpression(const Expr& expr);
    std::string getFormatSpecifier(const Expr& expr);
    std::string getBinOpString(BinOp op);
    std::string getUnOpString(UnOp op);
    std::string formatSpecifierFromType(const std::string& type);
    
    // Buffer management
    std::string getTempVarName();
};

} // namespace paani
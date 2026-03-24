#pragma once

#include "TemplateGenerator.hpp"
#include "../../paani_ast.hpp"
#include "../core/Naming.inl"
#include <sstream>
#include <iomanip>

namespace paani {

inline TemplateGenerator::TemplateGenerator(CodeGenContext& ctx, const std::string& packageName)
    : ctx_(ctx), packageName_(packageName) {}

inline void TemplateGenerator::generate(const TemplateDecl& decl) {
    std::string funcName = getSpawnFunctionName(decl.name);
    
    // Function signature
    ctx_.writeImplLn("paani_entity_t " + funcName + "(paani_world_t* __paani_gen_world) {");
    ctx_.pushIndent();
    
    // Spawn entity
    ctx_.writeImplLn("paani_entity_t e = paani_entity_create(__paani_gen_world);");
    ctx_.writeImplLn("if (e == (paani_entity_t)-1) return e;");
    ctx_.writeImplLn("");
    
    // Initialize components
    generateComponentInitializations(decl);
    
    // Return entity
    ctx_.writeImplLn("return e;");
    
    ctx_.popIndent();
    ctx_.writeImplLn("}");
}

// Helper: Generate expression value
inline std::string TemplateGenerator::generateExprValue(const Expr& expr) {
    std::ostringstream oss;
    
    if (auto* intExpr = dynamic_cast<const IntegerExpr*>(&expr)) {
        oss << intExpr->value;
    } else if (auto* floatExpr = dynamic_cast<const FloatExpr*>(&expr)) {
        // Ensure float output has decimal point
        oss << std::fixed << std::setprecision(1) << floatExpr->value;
    } else if (auto* strExpr = dynamic_cast<const StringExpr*>(&expr)) {
        oss << "\"" << strExpr->value << "\"";
    } else if (auto* boolExpr = dynamic_cast<const BoolExpr*>(&expr)) {
        oss << (boolExpr->value ? "1" : "0");
    } else if (auto* identExpr = dynamic_cast<const IdentifierExpr*>(&expr)) {
        oss << identExpr->name;
    } else {
        // For complex expressions, return a placeholder
        // In a full implementation, this would use CodeGenerator::genExpr
        oss << "/* complex expression */";
    }
    
    return oss.str();
}

inline void TemplateGenerator::generateComponentInitializations(const TemplateDecl& decl) {
    for (const auto& comp : decl.components) {
        std::string compPrefixed = getComponentPrefixedName(comp.componentName);
        
        ctx_.writeImplLn("// Initialize " + comp.componentName);
        ctx_.writeImplLn("{");
        ctx_.pushIndent();
        ctx_.writeImplLn(compPrefixed + " __comp_data = {0};");
        
        // Initialize fields
        for (const auto& field : comp.fieldValues) {
            std::string fieldName = field.first;
            // Generate expression for field value
            std::string value = generateExprValue(*field.second);
            ctx_.writeImplLn("__comp_data." + fieldName + " = " + value + ";");
        }
        
        ctx_.writeImplLn("paani_component_add(__paani_gen_world, e, s_ctype_" + compPrefixed + ", &__comp_data);");
        ctx_.popIndent();
        ctx_.writeImplLn("}");
        ctx_.writeImplLn("");
    }
}

inline std::string TemplateGenerator::getSpawnFunctionName(const std::string& templateName) {
    return Naming::templateSpawn(packageName_, templateName);
}

inline std::string TemplateGenerator::getComponentPrefixedName(const std::string& compName) {
    return Naming::component(packageName_, compName);
}

} // namespace paani

#pragma once

#include "../core/CodeGenContext.hpp"
#include "../../paani_ast.hpp"
#include <vector>
#include <string>

namespace paani {

// Forward declaration
class CodeGenerator;

// Template entity generator
class TemplateGenerator {
public:
    TemplateGenerator(CodeGenContext& ctx, const std::string& packageName);
    
    // Generate template spawn function
    void generate(const TemplateDecl& decl);
    
    // Set CodeGenerator for expression generation
    void setCodeGenerator(CodeGenerator* cg) {
        codeGen_ = cg;
    }
    
private:
    CodeGenContext& ctx_;
    std::string packageName_;
    CodeGenerator* codeGen_ = nullptr;
    
    // Generation phases
    void generateFunctionSignature(const TemplateDecl& decl);
    void generateComponentInitializations(const TemplateDecl& decl);
    void generateEntitySpawn(const TemplateDecl& decl);
    
    // Helper methods
    std::string getSpawnFunctionName(const std::string& templateName);
    std::string getComponentPrefixedName(const std::string& compName);
    std::string generateExprValue(const Expr& expr);
};

} // namespace paani

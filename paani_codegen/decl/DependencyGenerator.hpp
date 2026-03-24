#pragma once

#include "../core/CodeGenContext.hpp"
#include "../../paani_ast.hpp"
#include <string>

namespace paani {

// Dependency declaration generator
class DependencyGenerator {
public:
    DependencyGenerator(CodeGenContext& ctx, const std::string& packageName);
    
    // Generate dependency metadata
    void generate(const DependencyDecl& decl);
    
private:
    CodeGenContext& ctx_;
    std::string packageName_;
};

} // namespace paani

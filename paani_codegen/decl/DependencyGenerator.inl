#pragma once

#include "DependencyGenerator.hpp"

namespace paani {

inline DependencyGenerator::DependencyGenerator(CodeGenContext& ctx, const std::string& packageName)
    : ctx_(ctx), packageName_(packageName) {}

inline void DependencyGenerator::generate(const DependencyDecl& decl) {
    // NOTE: This method is currently not used.
    // Dependency generation is implemented in CodeGenerator::generateImpl.
    // This class exists for potential future refactoring.
    (void)decl;
    (void)packageName_;
}

} // namespace paani

#pragma once

#include "SystemGenerator.hpp"
#include "../core/Naming.inl"

namespace paani {

inline SystemGenerator::SystemGenerator(CodeGenContext& ctx,
                                         HandleTracker& handleTracker,
                                         const std::string& packageName,
                                         const SystemGenConfig& config)
    : ctx_(ctx)
    , handleTracker_(handleTracker)
    , packageName_(packageName)
    , config_(config)
{
}

inline void SystemGenerator::generate(const SystemDecl& decl) {
    std::string funcName = getSystemFunctionName(decl);
    std::string sig = "void " + funcName + 
                      "(paani_world_t* __paani_gen_world, float dt, void* userdata)";
    
    ctx_.writeImplLn(sig + " {");
    ctx_.pushIndent();
    
    // Setup handle tracking for system
    handleTracker_.beginFunction();
    
    if (decl.isTraitSystem()) {
        generateTraitSystem(decl);
    } else {
        generatePlainSystem(decl);
    }
    
    // Cleanup
    handleTracker_.endFunction();
    ctx_.popIndent();
    ctx_.writeImplLn("}");
}

inline void SystemGenerator::generateTraitSystem(const SystemDecl& decl) {
    // Clear deferred destroys at start
    ctx_.writeImplLn("paani_entity_clear_deferred_destroys(__paani_gen_world);");
    ctx_.writeImplLn("");
    
    // Get components for the trait
    std::vector<std::string> components;
    if (!decl.forTraits.empty()) {
        components = getTraitComponents(decl.forTraits[0]);
    }
    
    if (components.empty()) {
        // No components, generate simple query
        generateIteratorQuery(decl, components);
    } else if (config_.enableSoA && canVectorize(decl)) {
        // Use SoA optimization
        ctx_.writeImplLn("// Trait system (SoA optimized): " + decl.name);
        generateSoAQuery(decl, components);
    } else {
        // Use traditional iterator
        ctx_.writeImplLn("// Trait system: " + decl.name);
        generateIteratorQuery(decl, components);
    }
    
    // Process deferred destroys
    ctx_.writeImplLn("");
    ctx_.writeImplLn("paani_entity_process_deferred_destroys(__paani_gen_world);");
}

inline void SystemGenerator::generateSoAQuery(const SystemDecl& decl,
                                               const std::vector<std::string>& components) {
    // Build component types array
    ctx_.writeImplLn("// Component types for SoA query");
    ctx_.writeImpl("paani_ctype_t __comp_types[] = {");
    for (size_t i = 0; i < components.size(); i++) {
        if (i > 0) ctx_.writeImpl(", ");
        ctx_.writeImpl("s_ctype_" + getComponentPrefixedName(components[i]));
    }
    ctx_.writeImplLn("};");
    ctx_.writeImplLn("");
    
    // Get the world for the first component (they should all be in the same package)
    std::string queryWorld = components.empty() ? "__paani_gen_world" : getTraitWorld(decl.forTraits[0]);
    
    // Get SoA view using the correct world
    ctx_.writeImplLn("paani_query_soa_t __soa;");
    ctx_.writeImplLn("if (paani_query_soa_get(" + queryWorld + ", __comp_types, " +
                     std::to_string(components.size()) + ", &__soa)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn("");
    
    // Generate component column pointers
    generateComponentPointers(components, decl.entityVarName);
    ctx_.writeImplLn("");
    
    // Generate batch loop
    generateBatchLoop(decl, components);
    ctx_.writeImplLn("");
    
    // Release SoA view
    ctx_.writeImplLn("paani_query_soa_release(&__soa);");
    ctx_.popIndent();
    ctx_.writeImplLn("}");
}

inline void SystemGenerator::generateIteratorQuery(const SystemDecl& decl,
                                                    const std::vector<std::string>& components) {
    // Get trait type and world (handles cross-package access)
    std::string traitPrefixed = getTraitPrefixedName(decl.forTraits[0]);
    std::string traitWorld = getTraitWorld(decl.forTraits[0]);
    
    // Build query using the correct world for the trait
    ctx_.writeImplLn("// Query entities with trait " + decl.forTraits[0]);
    ctx_.writeImplLn("paani_query_t* __trait_query = paani_query_trait(" + traitWorld + ", s_trait_" + 
                     traitPrefixed + ");");
    ctx_.writeImplLn("paani_query_result_t __trait_result;");
    ctx_.writeImplLn("void* __trait_comp_buffer[" + std::to_string(components.size()) + "];");
    ctx_.writeImplLn("__trait_result.components = __trait_comp_buffer;");
    ctx_.writeImplLn("__trait_result.capacity = " + std::to_string(components.size()) + ";");
    ctx_.writeImplLn("");
    
    // Iterator loop
    ctx_.writeImplLn("while (paani_query_next(__trait_query, &__trait_result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn("paani_entity_t " + decl.entityVarName + " = __trait_result.entity;");
    ctx_.writeImplLn("(void)" + decl.entityVarName + ";");  // Silence unused warning
    
    // Generate component pointers
    for (size_t i = 0; i < components.size(); i++) {
        std::string compName = components[i];
        std::string compPrefixed = getComponentPrefixedName(compName);
        std::string pointerName = "_" + decl.entityVarName + "_" + compName;
        
        ctx_.writeImplLn(compPrefixed + "* " + pointerName + " = (" + compPrefixed + 
                         "*)__trait_result.components[" + std::to_string(i) + "];");
        componentPointers_[decl.entityVarName + "." + compName] = pointerName;
    }
    ctx_.writeImplLn("");
    
    // Generate body
    inTraitSystem_ = true;
    entityVarName_ = decl.entityVarName;
    generateSystemBody(decl);
    inTraitSystem_ = false;
    componentPointers_.clear();
    
    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.writeImplLn("");
    ctx_.writeImplLn("paani_query_destroy(__trait_query);");
}

inline void SystemGenerator::generatePlainSystem(const SystemDecl& decl) {
    ctx_.writeImplLn("// Plain system: " + decl.name);
    
    if (decl.body) {
        generateSystemBody(decl);
    }
}

inline void SystemGenerator::generateComponentPointers(const std::vector<std::string>& components,
                                                        const std::string& entityVar) {
    for (size_t i = 0; i < components.size(); i++) {
        std::string compName = components[i];
        std::string compPrefixed = getComponentPrefixedName(compName);
        std::string pointerName = "_" + entityVar + "_" + compName;
        
        ctx_.writeImplLn(compPrefixed + "* " + pointerName + " = (" + compPrefixed + 
                         "*)__soa.component_columns[" + std::to_string(i) + "];");
        componentPointers_[entityVar + "." + compName] = pointerName;
    }
}

inline void SystemGenerator::generateBatchLoop(const SystemDecl& decl,
                                                const std::vector<std::string>& components) {
    ctx_.writeImplLn("// Batch process all entities");
    ctx_.writeImplLn("for (uint32_t __i = 0; __i < __soa.count; __i++) {");
    ctx_.pushIndent();
    
    inSoAMode_ = true;
    inTraitSystem_ = true;
    entityVarName_ = decl.entityVarName;
    generateSystemBody(decl);
    inSoAMode_ = false;
    inTraitSystem_ = false;
    
    ctx_.popIndent();
    ctx_.writeImplLn("}");
}

inline void SystemGenerator::generateSystemBody(const SystemDecl& decl) {
    // NOTE: This method is currently not used.
    // System body generation is implemented directly in CodeGenerator::genSystemDecl.
    // This class exists for potential future refactoring.
    (void)decl;
}

inline std::vector<std::string> SystemGenerator::getTraitComponents(const std::string& traitName) {
    auto it = traitComponents_.find(traitName);
    if (it != traitComponents_.end()) {
        return it->second;
    }
    return {};
}

inline bool SystemGenerator::canVectorize(const SystemDecl& decl) {
    // Check if system can be vectorized
    // Requirements:
    // 1. Has traits
    // 2. Components are vectorizable (no pointers, simple types)
    // 3. No complex control flow
    
    if (decl.forTraits.empty()) return false;
    
    // Additional checks would go here
    // For now, enable SoA if config allows
    return config_.enableSoA;
}

inline std::string SystemGenerator::getSystemFunctionName(const SystemDecl& decl) {
    return Naming::system(packageName_, decl.name);
}

inline std::string SystemGenerator::getComponentPrefixedName(const std::string& compName) {
    // Check if component belongs to another package
    if (traitPackageMap_) {
        auto it = traitPackageMap_->find(compName);
        if (it != traitPackageMap_->end() && it->second != packageName_) {
            return Naming::component(it->second, compName);
        }
    }
    return Naming::component(packageName_, compName);
}

inline std::string SystemGenerator::getTraitPrefixedName(const std::string& traitName) {
    // Check if trait belongs to another package
    if (traitPackageMap_) {
        auto it = traitPackageMap_->find(traitName);
        if (it != traitPackageMap_->end() && it->second != packageName_) {
            return Naming::withPackage(it->second, traitName);
        }
    }
    return Naming::withPackage(packageName_, traitName);
}

// Get the world variable for a trait (handles cross-package access)
inline std::string SystemGenerator::getTraitWorld(const std::string& traitName) {
    if (traitPackageMap_) {
        auto it = traitPackageMap_->find(traitName);
        if (it != traitPackageMap_->end()) {
            if (it->second != packageName_) {
                // Cross-package access: use the target package's world variable
                return "__paani_gen_" + it->second + "_w";
            }
        }
    }
    // Same package: use current world parameter
    return "__paani_gen_world";
}

} // namespace paani

#pragma once

#include "QueryGenerator.hpp"
#include "../core/Naming.inl"

namespace paani {

inline QueryGenerator::QueryGenerator(CodeGenContext& ctx, const std::string& packageName)
    : ctx_(ctx), packageName_(packageName) {}

inline void QueryGenerator::generate(const QueryStmt& stmt) {
    if (stmt.traitNames.size() > 1) {
        generateMultiTraitQuery(stmt);
    } else if (!stmt.traitNames.empty()) {
        generateTraitQuery(stmt);
    } else {
        generateComponentQuery(stmt);
    }
}

inline void QueryGenerator::generateTraitQuery(const QueryStmt& stmt) {
    std::string traitName = stmt.traitNames[0];
    std::string traitPrefixed = getPrefixedName(traitName);
    std::string entityVar = stmt.varName.empty() ? "e" : stmt.varName;
    
    // Get component count from trait
    size_t compCount = 0;
    auto it = traitComponents_.find(traitName);
    if (it != traitComponents_.end()) {
        compCount = it->second.size();
    }
    // Ensure at least 1 for buffer declaration
    size_t bufferSize = compCount > 0 ? compCount : 1;

    ctx_.writeImplLn("// Query: trait " + traitName);
    ctx_.writeImplLn("paani_query_t* __query = paani_query_trait(__paani_gen_world, s_trait_" + traitPrefixed + ");");
    ctx_.writeImplLn("paani_query_result_t __result;");
    ctx_.writeImplLn("void* __comp_buffer[" + std::to_string(bufferSize) + "];");
    ctx_.writeImplLn("__result.components = __comp_buffer;");
    ctx_.writeImplLn("__result.capacity = " + std::to_string(bufferSize) + ";");
    ctx_.writeImplLn("");

    ctx_.writeImplLn("while (paani_query_next(__query, &__result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn("paani_entity_t " + entityVar + " = __result.entity;");
    ctx_.writeImplLn("(void)" + entityVar + ";");
    
    // NOTE: Query body generation is implemented in CodeGenerator::genQuery.
    // This class exists for potential future refactoring.
    
    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.writeImplLn("paani_query_destroy(__query);");
}

inline void QueryGenerator::generateComponentQuery(const QueryStmt& stmt) {
    // Query by components (without trait)
    std::string entityVar = stmt.varName.empty() ? "e" : stmt.varName;
    
    ctx_.writeImplLn("// Query: by components");
    
    if (stmt.withComponents.empty()) {
        ctx_.writeImplLn("// No components specified, query all entities");
        ctx_.writeImplLn("paani_query_t* __query = paani_query_create(__paani_gen_world, NULL, 0);");
    } else {
        // Build component types array
        ctx_.writeImplLn("// Component types for query");
        ctx_.writeImpl("paani_ctype_t __comp_types[] = {");
        for (size_t i = 0; i < stmt.withComponents.size(); i++) {
            if (i > 0) ctx_.writeImpl(", ");
            ctx_.writeImpl("s_ctype_" + getPrefixedName(stmt.withComponents[i]));
        }
        ctx_.writeImplLn("};");
        ctx_.writeImplLn("paani_query_t* __query = paani_query_create(__paani_gen_world, __comp_types, " + 
                         std::to_string(stmt.withComponents.size()) + ");");
    }
    
    ctx_.writeImplLn("paani_query_result_t __result;");
    size_t bufferSize = stmt.withComponents.empty() ? 1 : stmt.withComponents.size();
    ctx_.writeImplLn("void* __comp_buffer[" + std::to_string(bufferSize) + "];");
    ctx_.writeImplLn("__result.components = __comp_buffer;");
    ctx_.writeImplLn("__result.capacity = " + std::to_string(bufferSize) + ";");
    ctx_.writeImplLn("");
    ctx_.writeImplLn("while (paani_query_next(__query, &__result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn("paani_entity_t " + entityVar + " = __result.entity;");
    ctx_.writeImplLn("(void)" + entityVar + ";");
    
    // Generate component pointers if needed
    for (size_t i = 0; i < stmt.withComponents.size(); i++) {
        std::string compName = stmt.withComponents[i];
        std::string compPrefixed = getPrefixedName(compName);
        std::string pointerName = "_" + entityVar + "_" + compName;
        ctx_.writeImplLn(compPrefixed + "* " + pointerName + " = (" + compPrefixed + 
                         "*)__result.components[" + std::to_string(i) + "];");
    }
    if (!stmt.withComponents.empty()) ctx_.writeImplLn("");
    
    // NOTE: Query body generation is implemented in CodeGenerator::genQuery.
    // This class exists for potential future refactoring.
    (void)stmt;
    
    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.writeImplLn("paani_query_destroy(__query);");
}

inline void QueryGenerator::generateMultiTraitQuery(const QueryStmt& stmt) {
    // Query by multiple traits
    std::string entityVar = stmt.varName.empty() ? "e" : stmt.varName;
    
    ctx_.writeImplLn("// Multi-trait query: " + stmt.traitNames[0] + " + others");
    
    // For now, use the first trait for the main query
    // Additional traits would be filtered in the loop
    std::string firstTraitPrefixed = getPrefixedName(stmt.traitNames[0]);
    
    ctx_.writeImplLn("paani_query_t* __query = paani_query_trait(__paani_gen_world, s_trait_" + 
                     firstTraitPrefixed + ");");
    ctx_.writeImplLn("paani_query_result_t __result;");
    
    // Get max component count from all traits
    size_t maxCompCount = 0;
    for (const auto& traitName : stmt.traitNames) {
        auto it = traitComponents_.find(std::string(traitName));
        if (it != traitComponents_.end()) {
            maxCompCount = std::max(maxCompCount, it->second.size());
        }
    }
    size_t bufferSize = maxCompCount > 0 ? maxCompCount : 1;
    
    ctx_.writeImplLn("void* __comp_buffer[" + std::to_string(bufferSize) + "];");
    ctx_.writeImplLn("__result.components = __comp_buffer;");
    ctx_.writeImplLn("__result.capacity = " + std::to_string(bufferSize) + ";");
    ctx_.writeImplLn("");
    ctx_.writeImplLn("while (paani_query_next(__query, &__result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn("paani_entity_t " + entityVar + " = __result.entity;");
    ctx_.writeImplLn("(void)" + entityVar + ";");
    ctx_.writeImplLn("");
    
    // Filter by additional traits
    for (size_t i = 1; i < stmt.traitNames.size(); i++) {
        std::string traitPrefixed = getPrefixedName(stmt.traitNames[i]);
        ctx_.writeImplLn("// Check additional trait: " + std::string(stmt.traitNames[i]));
        ctx_.writeImplLn("if (!paani_entity_has_trait(__paani_gen_world, " + entityVar + 
                         ", s_trait_" + traitPrefixed + ")) continue;");
    }
    if (stmt.traitNames.size() > 1) ctx_.writeImplLn("");
    
    // Generate component pointers from first trait
    auto it = traitComponents_.find(std::string(stmt.traitNames[0]));
    if (it != traitComponents_.end()) {
        for (size_t i = 0; i < it->second.size(); i++) {
            std::string compName = it->second[i];
            std::string compPrefixed = getPrefixedName(compName);
            std::string pointerName = "_" + entityVar + "_" + compName;
            ctx_.writeImplLn(compPrefixed + "* " + pointerName + " = (" + compPrefixed + 
                             "*)__result.components[" + std::to_string(i) + "];");
        }
        if (!it->second.empty()) ctx_.writeImplLn("");
    }
    
    // NOTE: Query body generation is implemented in CodeGenerator::genQuery.
    // This class exists for potential future refactoring.
    (void)stmt;
    
    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.writeImplLn("paani_query_destroy(__query);");
}

inline std::string QueryGenerator::getPrefixedName(const std::string& name) {
    return Naming::withPackage(packageName_, name);
}

inline void QueryGenerator::generateQuerySetup(const std::vector<std::string>& traitNames) {
    (void)traitNames;
}

inline void QueryGenerator::generateQueryTeardown() {}

inline void QueryGenerator::generateLoopBody(const QueryStmt& stmt, const QueryResultInfo& resultInfo) {
    (void)stmt;
    (void)resultInfo;
}

} // namespace paani

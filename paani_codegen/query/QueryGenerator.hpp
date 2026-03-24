#pragma once

#include "../core/CodeGenContext.hpp"
#include "../../paani_ast.hpp"
#include <vector>
#include <string>
#include <unordered_map>

namespace paani {

// Forward declaration
class CodeGenerator;

// Query result structure
struct QueryResultInfo {
    std::string entityVar;
    std::vector<std::pair<std::string, std::string>> components;  // (name, pointerName)
};

// Query code generator
class QueryGenerator {
public:
    QueryGenerator(CodeGenContext& ctx, const std::string& packageName);
    
    // Generate query code
    void generate(const QueryStmt& stmt);
    
    // Set trait components mapping
    void setTraitComponents(const std::unordered_map<std::string, std::vector<std::string>>& comps) {
        traitComponents_ = comps;
    }
    
    // Set CodeGenerator for statement generation
    void setCodeGenerator(CodeGenerator* cg) {
        codeGen_ = cg;
    }
    
private:
    CodeGenContext& ctx_;
    std::string packageName_;
    std::unordered_map<std::string, std::vector<std::string>> traitComponents_;
    CodeGenerator* codeGen_ = nullptr;
    
    // Query types
    void generateTraitQuery(const QueryStmt& stmt);
    void generateComponentQuery(const QueryStmt& stmt);
    void generateMultiTraitQuery(const QueryStmt& stmt);
    
    // Helper methods
    std::string getPrefixedName(const std::string& name);
    void generateQuerySetup(const std::vector<std::string>& traitNames);
    void generateQueryTeardown();
    void generateLoopBody(const QueryStmt& stmt, const QueryResultInfo& resultInfo);
};

} // namespace paani

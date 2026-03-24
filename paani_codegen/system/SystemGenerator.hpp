#pragma once

#include "../core/CodeGenContext.hpp"
#include "../handle/HandleTracker.hpp"
#include "../../paani_ast.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace paani {

// Forward declaration
class CodeGenerator;

// System generation configuration
struct SystemGenConfig {
    bool enableSoA = true;           // Enable Structure of Arrays optimization
    bool enableVectorization = true; // Enable SIMD vectorization hints
    int batchSize = 1024;            // Batch processing size
};

// Component pointer information for trait systems
struct ComponentPointerInfo {
    std::string name;           // Component name
    std::string prefixedName;   // Prefixed component name (package__Component)
    std::string pointerName;    // Generated pointer variable name
    std::string fieldAccess;    // Field access pattern
    int typeIndex = -1;         // Index in component types array
};

// System code generator
class SystemGenerator {
public:
    SystemGenerator(CodeGenContext& ctx, 
                    HandleTracker& handleTracker,
                    const std::string& packageName,
                    const SystemGenConfig& config = {});
    
    // Main entry point
    void generate(const SystemDecl& decl);
    
    // Set trait components mapping
    void setTraitComponents(const std::unordered_map<std::string, std::vector<std::string>>& comps) {
        traitComponents_ = comps;
    }
    
    // Set trait package mapping for cross-package trait access
    void setTraitPackageMap(const std::unordered_map<std::string, std::string>* traitPackageMap) {
        traitPackageMap_ = traitPackageMap;
    }
    
    // Set CodeGenerator for statement generation
    void setCodeGenerator(CodeGenerator* cg) {
        codeGen_ = cg;
    }
    
private:
    CodeGenContext& ctx_;
    HandleTracker& handleTracker_;
    std::string packageName_;
    SystemGenConfig config_;
    CodeGenerator* codeGen_ = nullptr;
    
    // Component mapping
    std::unordered_map<std::string, std::vector<std::string>> traitComponents_;
    const std::unordered_map<std::string, std::string>* traitPackageMap_ = nullptr;
    std::unordered_map<std::string, std::string> componentPointers_;
    std::unordered_map<std::string, bool> vectorizedComponents_;
    
    // Generation state
    bool inSoAMode_ = false;
    bool inTraitSystem_ = false;
    std::string entityVarName_;
    
    // Generation phases
    void generateTraitSystem(const SystemDecl& decl);
    void generateSoAQuery(const SystemDecl& decl, const std::vector<std::string>& components);
    void generateIteratorQuery(const SystemDecl& decl, const std::vector<std::string>& components);
    void generatePlainSystem(const SystemDecl& decl);
    
    // Helper methods
    std::vector<std::string> getTraitComponents(const std::string& traitName);
    void generateComponentPointers(const std::vector<std::string>& components, 
                                    const std::string& entityVar);
    void generateBatchLoop(const SystemDecl& decl, const std::vector<std::string>& components);
    void generateIteratorLoop(const SystemDecl& decl, const std::vector<std::string>& components);
    void generateSystemBody(const SystemDecl& decl);
    
    // Code generation helpers
    std::string getSystemFunctionName(const SystemDecl& decl);
    std::string getComponentPrefixedName(const std::string& compName);
    std::string getTraitPrefixedName(const std::string& traitName);
    std::string getTraitWorld(const std::string& traitName);
    
    // SoA optimization helpers
    bool canVectorize(const SystemDecl& decl);
    void generateSoASetup(const std::vector<std::string>& components);
    void generateSoACleanup();
};

} // namespace paani

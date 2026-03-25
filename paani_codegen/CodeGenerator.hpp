// CodeGenerator.hpp - Refactored Code Generator
// Modular design with clear separation of concerns

#pragma once
#include "paani_ast.hpp"
#include "core/CodeGenContext.hpp"
#include "core/TypeMapper.hpp"
#include "core/Naming.hpp"
#include "handle/HandleTracker.hpp"
#include "handle/HandleEmitter.hpp"

// New modular generators
#include "system/SystemGenerator.hpp"
#include "decl/TemplateGenerator.hpp"
#include "decl/DependencyGenerator.hpp"

#include <memory>
#include <unordered_set>
#include <unordered_map>

namespace paani {

class CodeGenerator {
public:
    // Context and utilities
    CodeGenContext ctx_;
    TypeMapper typeMapper_;
    HandleTracker handleTracker_;
    std::unique_ptr<HandleEmitter> handleEmitter_;
    
    // Package info
    std::string packageName_;
    
    // Function tracking
    std::unordered_set<std::string> exportFunctions_;
    std::unordered_set<std::string> internalFunctions_;
    std::unordered_set<std::string> externFunctions_;
    std::unordered_map<std::string, TypeKind> functionReturnTypes_;
    std::unordered_map<std::string, std::string> functionReturnTypeNames_;
    
    // Type tracking
    std::unordered_set<std::string> externTypes_;
    std::unordered_map<std::string, std::string> handleDtors_;
    
    // Variable tracking
    std::unordered_map<std::string, std::string> varTypes_;
    
    // Import tracking
    std::unordered_set<std::string> usedPackages_;
    std::unordered_map<std::string, std::string> packageAliases_;
    
    // Trait components mapping (trait name -> component names)
    std::unordered_map<std::string, std::vector<std::string>> traitComponents_;
    
    // Trait package mapping (trait name -> package name) for cross-package trait access
    std::unordered_map<std::string, std::string> traitPackageMap_;
    
    // Component package mapping (component name -> package name) for cross-package component access
    std::unordered_map<std::string, std::string> dataPackageMap_;
    
    // Template package mapping (template name -> package name) for cross-package template access
    std::unordered_map<std::string, std::string> templatePackageMap_;
    
    // Entity world tracking (variable name -> world variable name)
    std::unordered_map<std::string, std::string> entityWorlds_;
    
    // Component field types mapping (componentName.fieldName -> C type)
    std::unordered_map<std::string, std::string> componentFieldTypes_;
    
    // New modular generators
    std::unique_ptr<SystemGenerator> systemGen_;
    std::unique_ptr<TemplateGenerator> templateGen_;
    std::unique_ptr<DependencyGenerator> depGen_;
    
    // State for expression generation
    int tempCounter_ = 0;
    bool inTraitSystem_ = false;
    bool inBatchSystem_ = false;  // True when generating batch-optimized system (SoA mode)
    std::unordered_map<std::string, std::string> componentPointers_;
    bool isEntryPoint_ = false;   // True if this is the entry point package (main)
    
public:
    // Package manager for accessing imported packages
    PackageManager packageManager;
    
    // Main entry points
    std::string generateHeader(const Package& pkg);
    std::string generateImpl(const Package& pkg);
    
    // Public interface for generators
    void generateStmt(const Stmt& stmt) { genStmt(stmt); }
    std::string generateExpr(const Expr& expr) { return genExpr(expr); }
    void setInTraitSystem(bool value) { inTraitSystem_ = value; }
    void setInBatchSystem(bool value) { inBatchSystem_ = value; }
    void setComponentPointers(const std::unordered_map<std::string, std::string>& ptrs) { componentPointers_ = ptrs; }
    void clearComponentPointers() { componentPointers_.clear(); }
    std::unordered_map<std::string, std::string>& getComponentPointers() { return componentPointers_; }
    
private:
    // Setup methods
    void setupPackage(const Package& pkg);
    void collectFunctionInfo(const Package& pkg);
    void collectExternInfo(const Package& pkg);
    void collectHandleInfo(const Package& pkg);
    void collectTraitInfo(const Package& pkg);
    void processImports(const Package& pkg);
    void initializeGenerators();
    
    // Declaration generators
    void genDataDecl(const DataDecl& decl);
    void genTraitDecl(const TraitDecl& decl);
    void genFnDecl(const FnDecl& decl, bool declarationOnly);
    void genExternFnDecl(const ExternFnDecl& decl);
    void genSystemDecl(const SystemDecl& decl);
    void generateSimpleTraitSystem(const SystemDecl& decl);
    void generateIteratorTraitSystem(const SystemDecl& decl, const std::vector<std::string>& components);
    void generateBatchTraitSystem(const SystemDecl& decl, const std::vector<std::string>& components);
    void genTemplateDecl(const TemplateDecl& decl);
    void genDependencyDecl(const DependencyDecl& decl);
    
    // Statement generators
    void genStmt(const Stmt& stmt);
    void genBlock(const BlockStmt& stmt);
    void genVarDecl(const VarDeclStmt& stmt);
    void genExprStmt(const ExprStmt& stmt);
    void genIf(const IfStmt& stmt);
    void genWhile(const WhileStmt& stmt);
    void genReturn(const ReturnStmt& stmt);
    void genBreak(const BreakStmt& stmt);
    void genContinue(const ContinueStmt& stmt);
    void genGive(const GiveStmt& stmt);
    void genTag(const TagStmt& stmt);
    void genUntag(const UntagStmt& stmt);
    void genDestroy(const DestroyStmt& stmt);
    void genQuery(const QueryStmt& stmt);
    void generateSingleTraitQueryImpl(const QueryStmt& stmt, const std::string& entityVar);
    void generateComponentQueryImpl(const QueryStmt& stmt, const std::string& entityVar);
    void generateMultiTraitQueryImpl(const QueryStmt& stmt, const std::string& entityVar);
    void genLock(const LockStmt& stmt);
    void genUnlock(const UnlockStmt& stmt);
    void genExit(const ExitStmt& stmt);
    
    // Expression generators
    std::string genExpr(const Expr& expr);
    std::string genCallExpr(const CallExpr& expr, bool isStmt = false);
    std::string genFStringExpr(const FStringExpr& expr);
    
    // Component access helper for trait systems
    std::string genComponentAccess(const std::string& entity, const std::string& component);
    
    // Entity world helper - get the world variable for an entity expression
    std::string getEntityWorld(const Expr& entityExpr);
    
    // Type helpers
    std::string mapType(const Type& type) const { return typeMapper_.map(type); }
    bool isHandleType(const Type& type) const { return typeMapper_.isHandle(type); }
    std::string inferType(const Expr& expr) const;
    
    // Handle management helpers
    void registerHandleVar(const std::string& name, const std::string& typeName);
    void emitHandleDtorsForCurrentScope();
    void emitAllHandleDtors();
    void emitHandleMove(const std::string& varName);
    
    // Entry point main generation
    void generateEntryMain(const Package& pkg);
};

} // namespace paani

// Include implementation
#include "CodeGenerator.inl"
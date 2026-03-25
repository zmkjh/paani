// paani_sema.hpp - Semantic Analyzer for Paani
// Provides strict semantic checking

#ifndef PAANI_SEMA_HPP
#define PAANI_SEMA_HPP

#include "paani_ast.hpp"
#include "paani_package.hpp"
#include "paani_colors.hpp"
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace paani {

// ============================================================================
// Semantic Error
// ============================================================================
class SemanticError : public std::runtime_error {
public:
    Location loc;

    SemanticError(const Location& location, const std::string& msg)
        : std::runtime_error(formatMessage(location, msg)),
          loc(location) {}

private:
    static std::string formatMessage(const Location& loc, const std::string& msg) {
        std::stringstream ss;
        ss << "Semantic error at line " << loc.line << ", column " << loc.column << ": " << msg;
        return ss.str();
    }
};

// ============================================================================
// Error Reporter
// ============================================================================
struct ErrorMessage {
    Location loc;
    std::string message;
    std::string suggestion;
    
    ErrorMessage(const Location& l, const std::string& m, const std::string& s = "")
        : loc(l), message(m), suggestion(s) {}
};

// ============================================================================
// Semantic Analyzer
// ============================================================================
class SemanticAnalyzer : public ASTVisitor {
public:
    void analyze(Package& pkg, const std::string& source = "");

    // Package manager for handling imports
    PackageManager packageManager;
    
    // Get formatted error report
    std::string getErrorReport(const std::string& filename = "") const;

private:
    // Source code for error context
    std::string sourceCode_;
    std::vector<std::string> sourceLines_;
    std::string filename_;
    // Symbol tables
    std::unordered_set<std::string> dataNames_;
    std::unordered_set<std::string> externTypeNames_;
    std::unordered_set<std::string> handleNames_;
    std::unordered_set<std::string> traitNames_;
    std::unordered_set<std::string> templateNames_;
    std::unordered_set<std::string> systemNames_;
    std::unordered_set<std::string> functionNames_;
    std::unordered_set<std::string> exportFunctionNames_;
    
    // Local (current package only) symbol tables for ECS entities
    // use statement can only bring in utils from non-entry files, not ECS systems/traits
    std::unordered_set<std::string> localSystemNames_;
    std::unordered_set<std::string> localTraitNames_;

    // Trait -> Components mapping
    std::unordered_map<std::string, std::vector<std::string>> traitComponents_;
    
    // Extern type -> size mapping
    std::unordered_map<std::string, uint32_t> externTypeSizes_;
    // Handle -> destructor mapping
    std::unordered_map<std::string, std::string> handleDtors_;
    std::unordered_set<std::string> externFunctionNames_;
    std::unordered_set<std::string> exportedExternTypes_;
    std::unordered_set<std::string> exportedExternFunctions_;

    // Variable scopes (stack of scopes)
    std::vector<std::unordered_set<std::string>> varScopes_;
    // Variable mutability tracking: true = mutable (var), false = immutable (let)
    std::vector<std::unordered_map<std::string, bool>> varMutability_;

    // Current context
    bool inTraitSystem_ = false;
    std::unordered_set<std::string> currentSystemComponents_;
    std::string currentEntityVarName_;  // Current entity variable name in trait system
    std::string currentPackageName_;
    bool isEntryPoint_ = false;
    // Array literal restriction: can only be used in variable initialization
    bool allowArrayLiteral_ = false;
    std::unordered_set<std::string> usedPackages_;
    std::unordered_map<std::string, std::string> dataPackageMap_;    // data name -> package
    std::unordered_map<std::string, std::string> traitPackageMap_;   // trait name -> package
    std::unordered_map<std::string, std::string> systemPackageMap_;  // system name -> package
    std::unordered_map<std::string, std::string> templatePackageMap_; // template name -> package
    std::unordered_map<std::string, std::string> packageAliases_;    // alias -> actual package name
    // Component field types: "ComponentName.fieldName" -> TypeKind
    std::unordered_map<std::string, TypeKind> componentFieldTypes_;
    // Array element types: "ComponentName.fieldName" -> base element TypeKind
    std::unordered_map<std::string, TypeKind> arrayElementTypes_;
    
    // Imported package symbols (for cross-package access control)
    struct ImportedPackage {
        std::string name;
        // Non-ECS symbols (available to all packages)
        std::unordered_set<std::string> functions;           // export fn
        std::unordered_set<std::string> externFunctions;     // export extern fn
        std::unordered_set<std::string> externTypes;         // export extern type
        std::unordered_set<std::string> handles;             // export handle
        // ECS symbols (only available to main package via package.Symbol)
        std::unordered_set<std::string> datas;               // export data
        std::unordered_set<std::string> traits;              // export trait
        std::unordered_set<std::string> templates;           // export template
    };
    std::unordered_map<std::string, ImportedPackage> importedPackages_;

    // Error tracking
    std::vector<ErrorMessage> errors_;
    bool hasError_ = false;

    // Type tracking - using full Type with world information
    std::vector<std::unordered_map<std::string, TypePtr>> varTypes_;
    std::unordered_map<std::string, TypeKind> functionReturnTypes_;
    std::unordered_map<std::string, std::vector<TypeKind>> functionParamTypes_;
    TypeKind currentReturnType_ = TypeKind::Void;

    // Helper methods
    void collectDeclarations(Package& pkg);
    void enterScope();
    void exitScope();
    void declareVariable(const std::string& name);
    void declareVariableWithMutability(const std::string& name, bool isMutable);
    bool isVariableMutable(const std::string& name);
    bool isVariableDeclared(const std::string& name);
    bool isDataDefined(const std::string& name);
    bool isDataDefinedInPackage(const std::string& package, const std::string& name);
    bool isTraitDefined(const std::string& name);
    bool isTraitDefinedInPackage(const std::string& package, const std::string& name);
    bool isTraitDefinedLocally(const std::string& name) const;
    bool isTemplateDefined(const std::string& name);
    bool isTemplateDefinedInPackage(const std::string& package, const std::string& name);
    bool isSystemDefined(const std::string& name);
    bool isFunctionDefined(const std::string& name);
    bool isTypeDefined(const Type& type);
    std::string getTypeName(const Type& type);
    void reportError(const Location& loc, const std::string& msg, const std::string& suggestion = "");
    void reportWarning(const Location& loc, const std::string& msg);
    
    // Check if a system can be vectorized (SoA optimized)
    bool canVectorizeSystem(const SystemDecl& node, std::string& reason);

    // Type checking methods
    TypeKind inferTypeKind(Expr& expr);
    TypeKind getVariableTypeKind(const std::string& name);
    void declareVariableWithType(const std::string& name, TypePtr type);
    void declareVariableWithTypeKind(const std::string& name, TypeKind kind);
    std::string getVariableWorld(const std::string& name);
    bool isTypeCompatible(TypeKind expected, TypeKind actual);
    bool isNumericType(TypeKind kind);
    bool isIntegerType(TypeKind kind);
    std::string typeKindToString(TypeKind kind);
    void checkTypeCompatibility(const Location& loc, TypeKind expected, TypeKind actual, const std::string& context);

    // F-string checking methods
    bool isFStringExpr(Expr& expr) const {
        return dynamic_cast<FStringExpr*>(&expr) != nullptr;
    }

    void checkFStringNotStored(Expr& expr, const Location& loc, const std::string& context) {
        if (isFStringExpr(expr)) {
            reportError(loc, context + " F-string can only be used directly as function arguments, not stored or returned.");
        }
    }

    // World checking methods (only for entry point / main package)
    std::string getDataPackage(const std::string& dataName);
    std::string getTraitPackage(const std::string& traitName);
    std::string getSystemPackage(const std::string& systemName);
    std::string getTemplatePackage(const std::string& templateName);
    void checkEntityWorldMatch(const Location& loc, const std::string& entityWorld, 
                               const std::string& itemName, const std::string& itemPackage,
                               const std::string& operation);
    void checkWorldConsistency(const Location& loc, const std::vector<std::string>& worlds,
                               const std::string& context);

    // Visitor implementations
    void visit(IdentifierExpr& node) override;
    // Literal expressions - no semantic check needed
    void visit(IntegerExpr& node) override {}  // Literal value, no check needed
    void visit(FloatExpr& node) override {}    // Literal value, no check needed
    void visit(StringExpr& node) override {}   // Literal value, no check needed
    void visit(FStringExpr& node) override;
    void visit(BoolExpr& node) override {}     // Literal value, no check needed
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(CallExpr& node) override;
    void visit(MemberExpr& node) override;
    void visit(IndexExpr& node) override;
    void visit(SpawnExpr& node) override;
    void visit(HasExpr& node) override;
    void visit(ArrayLiteralExpr& node) override;
    void visit(BlockStmt& node) override;
    void visit(VarDeclStmt& node) override;
    void visit(ExprStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(ReturnStmt& node) override;
    // Simple control flow - no additional semantic check needed
    void visit(BreakStmt& node) override {}      // Validated in parser
    void visit(ContinueStmt& node) override {}   // Validated in parser
    void visit(GiveStmt& node) override;
    void visit(TagStmt& node) override;
    void visit(UntagStmt& node) override;
    void visit(DestroyStmt& node) override;
    void visit(QueryStmt& node) override;
    void visit(LockStmt& node) override;
    void visit(UnlockStmt& node) override;
    void visit(DataDecl& node) override;
    void visit(TraitDecl& node) override;
    void visit(TemplateDecl& node) override;
    void visit(SystemDecl& node) override;
    void visit(DependencyDecl& node) override;
    void visit(FnDecl& node) override;
    void visit(ExternFnDecl& node) override {
        // Check for variadic parameters - not supported in Paani
        if (node.isVariadic) {
            reportError(node.loc, "Variadic parameters (...) are not supported. "
                       "Consider using fixed parameters or wrapper functions.");
        }
        
        // Check parameter types are defined
        for (const auto& param : node.params) {
            if (param.type && !isTypeDefined(*param.type)) {
                reportError(node.loc, "Undefined type in extern function parameter: " + getTypeName(*param.type));
            }
        }
        
        // Check return type is defined
        if (node.returnType && !isTypeDefined(*node.returnType)) {
            reportError(node.loc, "Undefined return type in extern function: " + getTypeName(*node.returnType));
        }
        
        externFunctionNames_.insert(node.name);
    }
    void visit(ExternTypeDecl& node) override;
    void visit(HandleDecl& node) override;
    void visit(ExportDecl& node) override;
    // Use declarations are processed by PackageManager during import resolution
    void visit(UseDecl& node) override {}  // Handled in analyze()
    void visit(Package& node) override;
    // Exit statement - only allowed in entry point (main package)
    void visit(ExitStmt& node) override {
        if (!isEntryPoint_) {
            reportError(node.loc, "'exit' statement can only be used in the entry point (main package)");
        }
    }
};

// Helper function to convert BinOp to string
inline std::string binOpToString(BinOp op) {
    switch (op) {
        case BinOp::Add: return "+";
        case BinOp::Sub: return "-";
        case BinOp::Mul: return "*";
        case BinOp::Div: return "/";
        case BinOp::Mod: return "%";
        case BinOp::Eq: return "==";
        case BinOp::Ne: return "!=";
        case BinOp::Lt: return "<";
        case BinOp::Gt: return ">";
        case BinOp::Le: return "<=";
        case BinOp::Ge: return ">=";
        case BinOp::And: return "&&";
        case BinOp::Or: return "||";
        case BinOp::Assign: return "=";
        case BinOp::AddAssign: return "+=";
        case BinOp::SubAssign: return "-=";
        default: return "?";
    }
}

// ============================================================================
// Implementations
// ============================================================================

inline void SemanticAnalyzer::analyze(Package& pkg, const std::string& source) {
    currentPackageName_ = pkg.name;
    isEntryPoint_ = pkg.isEntryPoint;
    sourceCode_ = source;
    
    // Split source into lines for error context
    sourceLines_.clear();
    if (!source.empty()) {
        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            sourceLines_.push_back(line);
        }
    }

    // First pass: collect all declarations
    collectDeclarations(pkg);

    // Second pass: check each declaration
    for (auto& data : pkg.datas) {
        data->accept(*this);
    }
    for (auto& trait : pkg.traits) {
        trait->accept(*this);
    }
    for (auto& tmpl : pkg.templates) {
        tmpl->accept(*this);
    }
    for (auto& sys : pkg.systems) {
        sys->accept(*this);
    }
    for (auto& fn : pkg.functions) {
        fn->accept(*this);
    }
    // Check global statements (entry point only)
    // Global statements share a single scope
    if (!pkg.globalStatements.empty()) {
        enterScope();
        for (auto& stmt : pkg.globalStatements) {
            stmt->accept(*this);
        }
        exitScope();
    }

    // Deferred check: handle destructor functions must be declared and have correct signature
    for (const auto& handle : pkg.handles) {
        if (!handle->dtor.empty()) {
            if (!externFunctionNames_.count(handle->dtor)) {
                reportError(handle->loc, "Handle destructor function '" + handle->dtor + "' is not declared. "
                           "Use 'extern fn " + handle->dtor + "(...);' to declare it.");
            } else {
                // Check destructor signature: must return void and have exactly 1 parameter
                auto retIt = functionReturnTypes_.find(handle->dtor);
                auto paramIt = functionParamTypes_.find(handle->dtor);
                
                if (retIt != functionReturnTypes_.end() && retIt->second != TypeKind::Void) {
                    reportError(handle->loc, "Handle destructor function '" + handle->dtor + "' must return 'void'.");
                }
                
                if (paramIt != functionParamTypes_.end() && paramIt->second.size() != 1) {
                    reportError(handle->loc, "Handle destructor function '" + handle->dtor + "' must have exactly 1 parameter.");
                }
            }
        }
    }
    
    // Deferred check: extern function parameter and return types
    for (const auto& ext : pkg.externFunctions) {
        // Check parameter types are defined
        for (const auto& param : ext->params) {
            if (param.type && !isTypeDefined(*param.type)) {
                reportError(ext->loc, "Undefined type in extern function parameter '" + param.name + "': " + getTypeName(*param.type));
            }
        }
        
        // Check return type is defined
        if (ext->returnType && !isTypeDefined(*ext->returnType)) {
            reportError(ext->loc, "Undefined return type in extern function: " + getTypeName(*ext->returnType));
        }
    }

    // Report errors if any
    if (hasError_) {
        throw std::runtime_error(getErrorReport());
    }
}

inline std::string SemanticAnalyzer::getErrorReport(const std::string& filename) const {
    std::stringstream ss;
    ss << "\n";
    
    for (size_t i = 0; i < errors_.size(); i++) {
        const auto& err = errors_[i];
        
        // Error header
        ss << "\033[1;31merror\033[0m[" << (i + 1) << "]: " << err.message << "\n";
        
        // Location
        if (!filename.empty()) {
            ss << "  \033[0;34m-->\033[0m " << filename << ":" << err.loc.line << ":" << err.loc.column << "\n";
        } else {
            ss << "  \033[0;34m-->\033[0m " << err.loc.line << ":" << err.loc.column << "\n";
        }
        
        // Source context
        if (!sourceLines_.empty() && err.loc.line > 0 && err.loc.line <= (int)sourceLines_.size()) {
            // Show context (2 lines before, error line, 2 lines after)
            int startLine = std::max(1, static_cast<int>(err.loc.line - 2));
            int endLine = std::min(static_cast<int>(sourceLines_.size()), static_cast<int>(err.loc.line + 2));
            
            ss << "   |\n";
            for (int lineNum = startLine; lineNum <= endLine; lineNum++) {
                // Line number
                ss << "\033[0;34m" << std::setw(3) << lineNum << " |\033[0m ";
                
                // Source line
                ss << sourceLines_[lineNum - 1] << "\n";
                
                // Error marker
                if (lineNum == err.loc.line) {
                    ss << "   | ";
                    // Add spaces for column offset
                    int col = err.loc.column;
                    // Adjust for tab characters
                    for (size_t j = 0; j < sourceLines_[lineNum - 1].size() && j < (size_t)(col - 1); j++) {
                        if (sourceLines_[lineNum - 1][j] == '\t') {
                            ss << "    ";
                            col -= 3;
                        } else {
                            ss << " ";
                        }
                    }
                    ss << "\033[1;31m^\033[0m\n";
                }
            }
            ss << "   |\n";
        }
        
        // Suggestion
        if (!err.suggestion.empty()) {
            ss << "  \033[0;32m= help:\033[0m " << err.suggestion << "\n";
        }
        
        if (i < errors_.size() - 1) {
            ss << "\n";
        }
    }
    
    ss << "\n";
    if (errors_.size() == 1) {
        ss << "\033[1;31merror:\033[0m aborting due to 1 previous error\n";
    } else {
        ss << "\033[1;31merror:\033[0m aborting due to " << errors_.size() << " previous errors\n";
    }
    
    return ss.str();
}

inline void SemanticAnalyzer::collectDeclarations(Package& pkg) {
    // Collect extern type definitions FIRST (before extern functions that may use them)
    for (const auto& extType : pkg.externTypes) {
        if (externTypeNames_.count(extType->name)) {
            reportError(extType->loc, "Duplicate extern type definition: " + extType->name);
        } else {
            externTypeNames_.insert(extType->name);
            externTypeSizes_[extType->name] = extType->size;
        }
    }

    // Collect extern function definitions
    for (const auto& ext : pkg.externFunctions) {
        // Check for variadic parameters - not supported in Paani
        if (ext->isVariadic) {
            reportError(ext->loc, "Variadic parameters (...) are not supported in extern functions. "
                       "Consider using fixed parameters or wrapper functions.");
        }

        if (functionNames_.count(ext->name)) {
            reportError(ext->loc, "Duplicate function definition: " + ext->name);
        }
        
        // Check parameter types are defined (deferred check after all types are collected)
        // Store for later check
        
        functionNames_.insert(ext->name);
        externFunctionNames_.insert(ext->name);  // Also track as extern function for handle checking

        // Collect extern function signature
        if (ext->returnType) {
            functionReturnTypes_[ext->name] = ext->returnType->kind;
        } else {
            functionReturnTypes_[ext->name] = TypeKind::Void;
        }
        std::vector<TypeKind> paramTypes;
        for (const auto& param : ext->params) {
            if (param.type) {
                paramTypes.push_back(param.type->kind);
            } else {
                paramTypes.push_back(TypeKind::Void);
            }
        }
        functionParamTypes_[ext->name] = paramTypes;
    }

    // Process uses - strict package access control
    // Main package: can load full package (access ECS objects via package.Symbol)
    // Non-main package: can ONLY load utils/ subdirectory (no ECS access)
    for (const auto& use : pkg.uses) {
        // Determine if we should only load utils/
        // Non-entry point files MUST use onlyUtils=true
        bool forceOnlyUtils = !isEntryPoint_;
        bool onlyUtils = forceOnlyUtils || use->onlyUtils;
        
        PackageInfo* importedPkg = nullptr;
        try {
            importedPkg = packageManager.loadPackageForUse(use->packagePath, onlyUtils);
        } catch (const std::exception& e) {
            reportError(use->loc, e.what());
            continue;
        }
        if (!importedPkg) {
            std::string pkgName;
            for (size_t i = 0; i < use->packagePath.size(); i++) {
                if (i > 0) pkgName += ".";
                pkgName += use->packagePath[i];
            }
            reportError(use->loc, "Cannot find package: " + pkgName);
            continue;
        }

        // Record used package for world checking
        std::string usedPkgName = importedPkg->name;
        usedPackages_.insert(usedPkgName);

        // Store package alias if present (e.g., "use engine as e")
        if (!use->alias.empty()) {
            packageAliases_[use->alias] = usedPkgName;
        }
        
        // Create imported package entry
        ImportedPackage imported;
        imported.name = usedPkgName;

        // Add non-ECS symbols (available to all packages)
        for (const auto& [funcName, funcType] : importedPkg->exportedFunctions) {
            std::string fullName = usedPkgName + "__" + funcName;
            functionNames_.insert(fullName);
            imported.functions.insert(funcName);
            
            if (funcType && funcType->funcReturn) {
                functionReturnTypes_[fullName] = funcType->funcReturn->kind;
            }
            
            std::vector<TypeKind> paramTypes;
            if (funcType) {
                for (const auto& param : funcType->funcParams) {
                    if (param) paramTypes.push_back(param->kind);
                }
            }
            functionParamTypes_[fullName] = paramTypes;
        }
        
        // Note: extern functions are NOT loaded from imported packages
        // They are private to the declaring package and cannot be accessed via 'use'
        
        for (const auto& [typeName, typeSize] : importedPkg->exportedExternTypes) {
            externTypeNames_.insert(typeName);
            externTypeSizes_[typeName] = typeSize;
            imported.externTypes.insert(typeName);
        }
        
        for (const auto& [handleName, dtor] : importedPkg->exportedHandles) {
            std::string fullName = usedPkgName + "__" + handleName;
            externTypeNames_.insert(fullName);  // Handle is treated as extern type
            imported.handles.insert(handleName);
        }
        
        // Add ECS symbols ONLY for main package
        // These are accessible via package.Symbol syntax
        if (isEntryPoint_) {
            for (const auto& [dataName, dataType] : importedPkg->exportedDatas) {
                imported.datas.insert(dataName);
            }
            
            for (const auto& [traitName, traitType] : importedPkg->exportedTraits) {
                imported.traits.insert(traitName);
                // Also load trait components for cross-package trait systems
                if (importedPkg->ast) {
                    for (const auto& trait : importedPkg->ast->traits) {
                        if (trait->name == traitName) {
                            traitComponents_[traitName] = trait->requiredData;
                            traitPackageMap_[traitName] = usedPkgName;
                            break;
                        }
                    }
                }
            }
            
            for (const auto& [tmplName, tmplInfo] : importedPkg->exportedTemplates) {
                imported.templates.insert(tmplName);
            }
        }
        
        importedPackages_[usedPkgName] = std::move(imported);
    }

    // Collect data definitions
    for (const auto& data : pkg.datas) {
        if (dataNames_.count(data->name)) {
            reportError(data->loc, "Duplicate data definition: " + data->name);
        }
        dataNames_.insert(data->name);
        dataPackageMap_[data->name] = pkg.name;
    }

    // Collect handle definitions (destructor check delayed to allow forward declaration)
    for (const auto& handle : pkg.handles) {
        if (handleNames_.count(handle->name)) {
            reportError(handle->loc, "Duplicate handle type definition: " + handle->name);
        } else {
            handleNames_.insert(handle->name);
            handleDtors_[handle->name] = handle->dtor;
        }
    }

    // Collect trait definitions
    for (const auto& trait : pkg.traits) {
        if (traitNames_.count(trait->name)) {
            reportError(trait->loc, "Duplicate trait definition: " + trait->name);
        }
        traitPackageMap_[trait->name] = pkg.name;
        traitNames_.insert(trait->name);
        localTraitNames_.insert(trait->name);  // Mark as local (current package)
        traitComponents_[trait->name] = trait->requiredData;
    }

    // Collect template definitions
    for (const auto& tmpl : pkg.templates) {
        if (templateNames_.count(tmpl->name)) {
            reportError(tmpl->loc, "Duplicate template definition: " + tmpl->name);
        }
        templateNames_.insert(tmpl->name);
        templatePackageMap_[tmpl->name] = pkg.name;
    }

    // Collect system definitions
    for (const auto& sys : pkg.systems) {
        if (systemNames_.count(sys->name)) {
            reportError(sys->loc, "Duplicate system definition: " + sys->name);
        }
        systemNames_.insert(sys->name);
        localSystemNames_.insert(sys->name);  // Mark as local (current package)
        systemPackageMap_[sys->name] = pkg.name;
    }

    // Collect function definitions
    // Local functions are stored with package prefix for consistency
    for (const auto& fn : pkg.functions) {
        std::string fullFuncName = pkg.name + "__" + fn->name;
        if (functionNames_.count(fullFuncName)) {
            reportError(fn->loc, "Duplicate function definition: " + fn->name);
        }
        functionNames_.insert(fullFuncName);
        if (fn->isExport) {
            exportFunctionNames_.insert(fullFuncName);
        }

        // Collect function signature
        if (fn->returnType) {
            functionReturnTypes_[fullFuncName] = fn->returnType->kind;
        } else {
            functionReturnTypes_[fullFuncName] = TypeKind::Void;
        }
        std::vector<TypeKind> paramTypes;
        
        // For export functions, add implicit 'world' parameter first
        if (fn->isExport) {
            paramTypes.push_back(TypeKind::Pointer);  // paani_world_t*
        }
        
        for (const auto& param : fn->params) {
            if (param.type) {
                paramTypes.push_back(param.type->kind);
            } else {
                paramTypes.push_back(TypeKind::Void);
            }
        }
        functionParamTypes_[fullFuncName] = std::move(paramTypes);
    }
}

inline void SemanticAnalyzer::enterScope() {
    varScopes_.emplace_back();
    varTypes_.emplace_back();
    varMutability_.emplace_back();
}

inline void SemanticAnalyzer::exitScope() {
    if (!varScopes_.empty()) {
        varScopes_.pop_back();
    }
    if (!varTypes_.empty()) {
        varTypes_.pop_back();
    }
    if (!varMutability_.empty()) {
        varMutability_.pop_back();
    }
}

inline void SemanticAnalyzer::declareVariable(const std::string& name) {
    // Check for reserved prefix used by compiler-generated code
    if (name.find("__paani_gen_") == 0) {
        reportError(Location(), "Variable name '" + name + "' uses reserved prefix '__paani_gen_'");
        return;
    }
    if (!varScopes_.empty()) {
        // Check if variable already exists in current scope (redefinition)
        if (varScopes_.back().count(name)) {
            reportError(Location(), "Variable '" + name + "' is already declared in this scope");
            return;
        }
        varScopes_.back().insert(name);
    }
}

// Declare variable with mutability information (true = mutable/var, false = immutable/let)
inline void SemanticAnalyzer::declareVariableWithMutability(const std::string& name, bool isMutable) {
    declareVariable(name);
    if (!varMutability_.empty()) {
        varMutability_.back()[name] = isMutable;
    }
}

// Check if variable is mutable (returns true if mutable or not found - for undefined vars)
inline bool SemanticAnalyzer::isVariableMutable(const std::string& name) {
    for (auto it = varMutability_.rbegin(); it != varMutability_.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            return varIt->second;
        }
    }
    return true; // Default to mutable for undefined variables (error will be reported elsewhere)
}

inline bool SemanticAnalyzer::isVariableDeclared(const std::string& name) {
    // Note: We used to check for implicit 'e' variable here, but now entity variable names
    // are explicit and declared via declareVariable().
    // Check scopes from inner to outer
    for (auto it = varScopes_.rbegin(); it != varScopes_.rend(); ++it) {
        if (it->count(name)) {
            return true;
        }
    }
    return false;
}

inline bool SemanticAnalyzer::isDataDefined(const std::string& name) {
    // Check if name has package prefix (e.g., "engine.PlayerData")
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string pkgName = name.substr(0, dotPos);
        std::string dataName = name.substr(dotPos + 1);
        return isDataDefinedInPackage(pkgName, dataName);
    }
    
    // Check local data first
    if (dataNames_.count(name)) return true;
    
    // For main package, also check imported packages (for cross-package ECS access)
    if (isEntryPoint_) {
        for (const auto& [pkgName, imported] : importedPackages_) {
            if (imported.datas.count(name)) return true;
        }
    }
    
    return false;
}

// Check if data is defined in a specific package (for package.Symbol access)
inline bool SemanticAnalyzer::isDataDefinedInPackage(const std::string& package, const std::string& name) {
    auto it = importedPackages_.find(package);
    if (it != importedPackages_.end()) {
        return it->second.datas.count(name);
    }
    return false;
}

inline bool SemanticAnalyzer::isTraitDefined(const std::string& name) {
    // Check if name has package prefix (e.g., "engine.PlayerTrait")
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string pkgName = name.substr(0, dotPos);
        std::string traitName = name.substr(dotPos + 1);
        return isTraitDefinedInPackage(pkgName, traitName);
    }
    
    // Check local traits first
    if (traitNames_.count(name)) return true;
    
    // For main package, also check imported packages (for cross-package ECS access)
    // Note: This is used for explicit package.Trait access, not implicit
    if (isEntryPoint_) {
        for (const auto& [pkgName, imported] : importedPackages_) {
            if (imported.traits.count(name)) return true;
        }
    }
    
    return false;
}

// Check if trait is defined LOCALLY (current package only)
inline bool SemanticAnalyzer::isTraitDefinedLocally(const std::string& name) const {
    return traitNames_.count(name);
}

// Check if trait is defined in a specific package (for package.Symbol access)
inline bool SemanticAnalyzer::isTraitDefinedInPackage(const std::string& package, const std::string& name) {
    auto it = importedPackages_.find(package);
    if (it != importedPackages_.end()) {
        return it->second.traits.count(name);
    }
    return false;
}

inline bool SemanticAnalyzer::isTemplateDefined(const std::string& name) {
    return templateNames_.count(name);
}

// Check if template is defined in a specific package (for package.Symbol access)
inline bool SemanticAnalyzer::isTemplateDefinedInPackage(const std::string& package, const std::string& name) {
    auto it = importedPackages_.find(package);
    if (it != importedPackages_.end()) {
        return it->second.templates.count(name);
    }
    return false;
}

inline bool SemanticAnalyzer::isSystemDefined(const std::string& name) {
    return systemNames_.count(name);
}

inline bool SemanticAnalyzer::isFunctionDefined(const std::string& name) {
    return functionNames_.count(name);
}

// Check if a type is defined (for UserDefined types)
inline bool SemanticAnalyzer::isTypeDefined(const Type& type) {
    switch (type.kind) {
        case TypeKind::Void:
        case TypeKind::Bool:
        case TypeKind::I8:
        case TypeKind::I16:
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::U8:
        case TypeKind::U16:
        case TypeKind::U32:
        case TypeKind::U64:
        case TypeKind::F32:
        case TypeKind::F64:
        case TypeKind::String:
        case TypeKind::Entity:
        case TypeKind::Handle:
            return true;
        case TypeKind::UserDefined:
        case TypeKind::ExternType:
            return dataNames_.count(type.name) || externTypeNames_.count(type.name) || 
                   handleNames_.count(type.name) || traitNames_.count(type.name);
        case TypeKind::Pointer:
            return type.base ? isTypeDefined(*type.base) : true;
        case TypeKind::Array:
            return type.base ? isTypeDefined(*type.base) : true;
        case TypeKind::Function:
            return true;  // Function types are always valid
        default:
            return false;
    }
}

// Get type name for error messages
inline std::string SemanticAnalyzer::getTypeName(const Type& type) {
    switch (type.kind) {
        case TypeKind::Void: return "void";
        case TypeKind::Bool: return "bool";
        case TypeKind::I8: return "i8";
        case TypeKind::I16: return "i16";
        case TypeKind::I32: return "i32";
        case TypeKind::I64: return "i64";
        case TypeKind::U8: return "u8";
        case TypeKind::U16: return "u16";
        case TypeKind::U32: return "u32";
        case TypeKind::U64: return "u64";
        case TypeKind::F32: return "f32";
        case TypeKind::F64: return "f64";
        case TypeKind::String: return "string";
        case TypeKind::Entity: return "entity";
        case TypeKind::Handle: return "handle";
        case TypeKind::UserDefined:
        case TypeKind::ExternType:
            return type.name;
        case TypeKind::Pointer:
            return "*" + (type.base ? getTypeName(*type.base) : "?");
        case TypeKind::Array:
            return "[" + (type.base ? getTypeName(*type.base) : "?") + "; " + std::to_string(type.arraySize) + "]";
        case TypeKind::Function:
            return "fn";
        default:
            return "unknown";
    }
}

// World checking methods (only for entry point / main package)
inline std::string SemanticAnalyzer::getDataPackage(const std::string& dataName) {
    // Check if name has package prefix (e.g., "engine.PlayerData")
    size_t dotPos = dataName.find('.');
    if (dotPos != std::string::npos) {
        std::string pkgName = dataName.substr(0, dotPos);
        return pkgName;
    }
    
    auto it = dataPackageMap_.find(dataName);
    if (it != dataPackageMap_.end()) return it->second;
    return currentPackageName_;
}

inline std::string SemanticAnalyzer::getTraitPackage(const std::string& traitName) {
    // Check if name has package prefix (e.g., "engine.PlayerTrait")
    size_t dotPos = traitName.find('.');
    if (dotPos != std::string::npos) {
        std::string pkgName = traitName.substr(0, dotPos);
        return pkgName;
    }
    
    auto it = traitPackageMap_.find(traitName);
    if (it != traitPackageMap_.end()) return it->second;
    return currentPackageName_;
}

inline std::string SemanticAnalyzer::getSystemPackage(const std::string& systemName) {
    auto it = systemPackageMap_.find(systemName);
    if (it != systemPackageMap_.end()) return it->second;
    return currentPackageName_;
}

inline std::string SemanticAnalyzer::getTemplatePackage(const std::string& templateName) {
    auto it = templatePackageMap_.find(templateName);
    if (it != templatePackageMap_.end()) return it->second;
    return currentPackageName_;
}

inline void SemanticAnalyzer::checkEntityWorldMatch(const Location& loc, const std::string& entityWorld,
                                                    const std::string& itemName, const std::string& itemPackage,
                                                    const std::string& operation) {
    if (!isEntryPoint_) return;  // Only check in main package

    std::string entityPkg = entityWorld.empty() ? currentPackageName_ : entityWorld;

    if (entityPkg != itemPackage) {
        reportError(loc, "World mismatch: Cannot " + operation + " '" + itemName +
                   "' (from package '" + itemPackage + "') on entity from package '" + entityPkg + "'. " +
                   "Each entity belongs to exactly one world.");
    }
}

inline void SemanticAnalyzer::checkWorldConsistency(const Location& loc, const std::vector<std::string>& worlds,
                                                    const std::string& context) {
    if (!isEntryPoint_) return;  // Only check in main package
    if (worlds.size() <= 1) return;

    std::string firstWorld = worlds[0].empty() ? currentPackageName_ : worlds[0];
    for (size_t i = 1; i < worlds.size(); i++) {
        std::string world = worlds[i].empty() ? currentPackageName_ : worlds[i];
        if (world != firstWorld) {
            reportError(loc, "World mismatch in " + context + ": Cannot mix entities from different worlds ('" +
                       firstWorld + "' and '" + world + "').");
            return;
        }
    }
}

inline void SemanticAnalyzer::reportError(const Location& loc, const std::string& msg, const std::string& suggestion) {
    errors_.emplace_back(loc, msg, suggestion);
    hasError_ = true;
}

inline void SemanticAnalyzer::reportWarning(const Location& loc, const std::string& msg) {
    // Print warning to stderr with color
    ConsoleColor::printWarningPrefix();
    std::cerr << " Line " << loc.line << ", Column " << loc.column << ": " << msg << "\n";
}

// Visitor implementations

inline void SemanticAnalyzer::visit(IdentifierExpr& node) {
    if (!isVariableDeclared(node.name)) {
        // Check if it's a function call (will be handled in CallExpr)
        if (!isFunctionDefined(node.name) && !isDataDefined(node.name) &&
            !isTraitDefined(node.name) && !isTemplateDefined(node.name)) {
            reportError(node.loc, "Undefined identifier: " + node.name);
        }
    }
}

inline void SemanticAnalyzer::visit(FStringExpr& node) {
    // Visit all expression parts to check for errors
    for (auto& part : node.parts) {
        if (part.isExpr && part.expr) {
            part.expr->accept(*this);
        }
    }
}

inline void SemanticAnalyzer::visit(BinaryExpr& node) {
    // Binary expressions don't allow array literals in their operands
    // (array literals can only be used directly in variable initialization)
    bool savedAllowArrayLiteral = allowArrayLiteral_;
    allowArrayLiteral_ = false;
    node.left->accept(*this);
    node.right->accept(*this);
    allowArrayLiteral_ = savedAllowArrayLiteral;

    // Type checking for binary operations
    auto leftType = inferTypeKind(*node.left);
    auto rightType = inferTypeKind(*node.right);

    switch (node.op) {
        case BinOp::Add:
            // Addition: numeric types only
            // String concatenation is not supported (requires memory management)
            if (leftType == TypeKind::String || rightType == TypeKind::String) {
                reportError(node.loc, "String concatenation is not supported. Consider using format functions in C host code.");
                break;
            }
            if (!isNumericType(leftType)) {
                reportError(node.loc, "Left operand of '+' must be numeric, got " + typeKindToString(leftType));
            }
            if (!isNumericType(rightType)) {
                reportError(node.loc, "Right operand of '+' must be numeric, got " + typeKindToString(rightType));
            }
            break;

        case BinOp::Sub:
        case BinOp::Mul:
        case BinOp::Div:
        case BinOp::Mod:
            // Arithmetic operations require numeric types
            if (!isNumericType(leftType)) {
                reportError(node.loc, "Left operand of arithmetic operation must be numeric, got " + typeKindToString(leftType));
            }
            if (!isNumericType(rightType)) {
                reportError(node.loc, "Right operand of arithmetic operation must be numeric, got " + typeKindToString(rightType));
            }
            break;

        case BinOp::Eq:
        case BinOp::Ne:
            // Equality can compare any compatible types
            if (!isTypeCompatible(leftType, rightType)) {
                reportError(node.loc, "Cannot compare " + typeKindToString(leftType) + " with " + typeKindToString(rightType));
            }
            break;

        case BinOp::Lt:
        case BinOp::Gt:
        case BinOp::Le:
        case BinOp::Ge:
            // Ordering comparison requires numeric types
            if (!isNumericType(leftType)) {
                reportError(node.loc, "Left operand of ordering comparison must be numeric, got " + typeKindToString(leftType));
            }
            if (!isNumericType(rightType)) {
                reportError(node.loc, "Right operand of ordering comparison must be numeric, got " + typeKindToString(rightType));
            }
            break;

        case BinOp::And:
        case BinOp::Or:
            // Logical operations require boolean types
            if (leftType != TypeKind::Bool) {
                reportError(node.loc, "Left operand of logical operation '" + binOpToString(node.op) + "' must be bool, got " + typeKindToString(leftType));
            }
            if (rightType != TypeKind::Bool) {
                reportError(node.loc, "Right operand of logical operation '" + binOpToString(node.op) + "' must be bool, got " + typeKindToString(rightType));
            }
            break;

        case BinOp::Assign:
            // Assignment requires compatible types
            if (!isTypeCompatible(leftType, rightType)) {
                reportError(node.loc, "Cannot assign " + typeKindToString(rightType) + " to " + typeKindToString(leftType));
            }
            // Check mutability: cannot assign to let (immutable) variables
            if (auto* idExpr = dynamic_cast<IdentifierExpr*>(node.left.get())) {
                if (isVariableDeclared(idExpr->name) && !isVariableMutable(idExpr->name)) {
                    reportError(node.loc, "Cannot assign to immutable variable '" + idExpr->name + "'. Use 'var' instead of 'let' to make it mutable.");
                }
            }
            // Check: cannot assign to string index (strings are immutable in Paani)
            if (auto* idxExpr = dynamic_cast<IndexExpr*>(node.left.get())) {
                auto objType = inferTypeKind(*idxExpr->object);
                if (objType == TypeKind::String) {
                    reportError(node.loc, "Cannot assign to string index. Strings are immutable in Paani.");
                }
            }
            // Check: f-string cannot be assigned (to variable, array element, or struct field)
            checkFStringNotStored(*node.right, node.loc, "Cannot assign f-string:");
            break;

        case BinOp::AddAssign:
        case BinOp::SubAssign:
            // Compound assignment requires numeric types
            if (!isNumericType(leftType)) {
                reportError(node.loc, "Left operand of compound assignment must be numeric, got " + typeKindToString(leftType));
            }
            if (!isTypeCompatible(leftType, rightType)) {
                reportError(node.loc, "Cannot assign " + typeKindToString(rightType) + " to " + typeKindToString(leftType) + " in compound assignment");
            }
            // Check mutability: cannot use compound assignment on let (immutable) variables
            if (auto* idExpr = dynamic_cast<IdentifierExpr*>(node.left.get())) {
                if (isVariableDeclared(idExpr->name) && !isVariableMutable(idExpr->name)) {
                    reportError(node.loc, "Cannot use compound assignment on immutable variable '" + idExpr->name + "'. Use 'var' instead of 'let' to make it mutable.");
                }
            }
            // Check: cannot use compound assignment on string index (strings are immutable)
            if (auto* idxExpr = dynamic_cast<IndexExpr*>(node.left.get())) {
                auto objType = inferTypeKind(*idxExpr->object);
                if (objType == TypeKind::String) {
                    reportError(node.loc, "Cannot use compound assignment on string index. Strings are immutable in Paani.");
                }
            }
            break;
    }
}

inline void SemanticAnalyzer::visit(UnaryExpr& node) {
    node.operand->accept(*this);

    auto operandType = inferTypeKind(*node.operand);

    switch (node.op) {
        case UnOp::Neg:
            // Negation requires numeric type
            if (!isNumericType(operandType)) {
                reportError(node.loc, "Unary negation requires numeric type, got " + typeKindToString(operandType));
            }
            break;

        case UnOp::Not:
            // Logical not requires boolean type
            if (operandType != TypeKind::Bool) {
                reportError(node.loc, "Logical not requires bool type, got " + typeKindToString(operandType));
            }
            break;

        case UnOp::Deref:
        case UnOp::AddrOf:
            // Pointer operations - for now, accept any type
            break;
    }
}

inline void SemanticAnalyzer::visit(CallExpr& node) {
    // Function call arguments don't allow array literals
    bool savedAllowArrayLiteral = allowArrayLiteral_;
    allowArrayLiteral_ = false;

    // Check if callee is a function
    if (auto* idExpr = dynamic_cast<IdentifierExpr*>(node.callee.get())) {
        std::string funcName = idExpr->name;
        
        // Try with package prefix first (for local functions)
        std::string fullFuncName = currentPackageName_ + "__" + funcName;
        bool isLocalFunction = isFunctionDefined(fullFuncName);
        bool isImportedFunction = isFunctionDefined(funcName);  // Imported functions already have prefix

        // Check if it's a template spawn (not a regular function call)
        if (!isLocalFunction && !isImportedFunction && !isTemplateDefined(funcName)) {
            std::string suggestion = "Did you forget to use the package? Use 'use package;' or check the function name.";
            if (funcName.find("__") != std::string::npos) {
                suggestion = "Use package-qualified syntax: 'package.function(...)' instead of direct call.";
            }
            reportError(node.loc, "Undefined function or template: " + funcName, suggestion);
        }

        // Check argument types if function is defined
        if (isLocalFunction || isImportedFunction) {
            std::string lookupName = isLocalFunction ? fullFuncName : funcName;
            auto it = functionParamTypes_.find(lookupName);
            if (it != functionParamTypes_.end()) {
                const auto& expectedParams = it->second;
                // Note: export functions have implicit 'world' parameter, so we don't count it
                size_t implicitParams = 0;
                if (exportFunctionNames_.count(lookupName)) {
                    implicitParams = 1;  // Skip world parameter
                }

                size_t numUserArgs = node.args.size();
                size_t numUserParams = expectedParams.size() > implicitParams ? expectedParams.size() - implicitParams : 0;

                if (numUserArgs != numUserParams) {
                    reportError(node.loc, "Function '" + funcName + "' expects " +
                               std::to_string(numUserParams) + " arguments, got " +
                               std::to_string(numUserArgs));
                } else {
                    // Check argument types (skip implicit world parameter for export functions)
                    for (size_t i = 0; i < node.args.size(); i++) {
                        auto argType = inferTypeKind(*node.args[i]);
                        size_t paramIndex = i + implicitParams;
                        if (paramIndex < expectedParams.size()) {
                            checkTypeCompatibility(node.args[i]->loc, expectedParams[paramIndex], argType,
                                "Argument " + std::to_string(i + 1) + " of '" + funcName + "'");
                        }
                    }
                }
            }
        }

        // Check arguments
        for (auto& arg : node.args) {
            arg->accept(*this);
        }
    } else if (auto* memberExpr = dynamic_cast<MemberExpr*>(node.callee.get())) {
        // Check if this is a package-qualified function call (e.g., engine.clamp(x))
        std::vector<std::string> pathParts;
        const Expr* current = node.callee.get();
        while (auto* me = dynamic_cast<const MemberExpr*>(current)) {
            pathParts.push_back(me->member);
            current = me->object.get();
        }
        if (auto* idExpr = dynamic_cast<const IdentifierExpr*>(current)) {
            pathParts.push_back(idExpr->name);
        }
        std::reverse(pathParts.begin(), pathParts.end());
        
        // If we have at least 2 parts, it might be a package-qualified function
        if (pathParts.size() >= 2) {
            // Resolve package alias if present (e.g., "e.clamp" -> "engine.clamp")
            std::string packageName = pathParts[0];
            auto aliasIt = packageAliases_.find(packageName);
            if (aliasIt != packageAliases_.end()) {
                packageName = aliasIt->second;
            }
            
            std::string fullFuncName = packageName;
            for (size_t i = 1; i < pathParts.size(); i++) {
                fullFuncName += "__" + pathParts[i];
            }
            
            // Check if the function is defined and exported
            std::string funcName = pathParts.back();
            
            // Check if trying to call extern function from another package
            // extern functions are private to the declaring package
            auto pkgIt = importedPackages_.find(packageName);
            if (pkgIt != importedPackages_.end()) {
                // Cross-package call - extern functions are not accessible
                if (externFunctionNames_.count(funcName)) {
                    reportError(node.loc, "Cannot access extern function '" + funcName + "' from package '" + packageName + "'. Extern functions are private to the declaring package.");
                } else if (!isFunctionDefined(fullFuncName)) {
                    reportError(node.loc, "Function '" + funcName + "' is not exported from package '" + packageName + "'");
                } else {
                    // Check if function is actually exported
                    const auto& imported = pkgIt->second;
                    if (!imported.functions.count(funcName)) {
                        reportError(node.loc, "Function '" + funcName + "' is not exported from package '" + packageName + "'");
                    }
                }
            } else if (!isFunctionDefined(fullFuncName)) {
                reportError(node.loc, "Undefined function: " + fullFuncName);
            }
        }
        
        // Check arguments
        for (auto& arg : node.args) {
            arg->accept(*this);
        }
    } else {
        // Complex callee expression
        node.callee->accept(*this);
        for (auto& arg : node.args) {
            arg->accept(*this);
        }
    }

    // Restore array literal allowance
    allowArrayLiteral_ = savedAllowArrayLiteral;
}

inline void SemanticAnalyzer::visit(MemberExpr& node) {
    // Member expression object doesn't allow array literals
    bool savedAllowArrayLiteral = allowArrayLiteral_;
    allowArrayLiteral_ = false;

    // Check if this is a package-qualified access (e.g., engine.Position)
    // In this case, the object chain represents package path, not local variables
    bool isPackageQualified = false;
    std::string packageName;

    // Walk up the member chain to see if this looks like a package path
    const Expr* current = &node;
    while (auto* me = dynamic_cast<const MemberExpr*>(current)) {
        current = me->object.get();
    }

    // If the root is an identifier that's not defined locally, it might be a package name
    if (auto* idExpr = dynamic_cast<const IdentifierExpr*>(current)) {
        std::string rootName = idExpr->name;
        // Check if it's a package alias first
        auto aliasIt = packageAliases_.find(rootName);
        if (aliasIt != packageAliases_.end()) {
            packageName = aliasIt->second;
            isPackageQualified = true;
        } else if (importedPackages_.count(rootName)) {
            packageName = rootName;
            isPackageQualified = true;
        }
    }

    if (isPackageQualified) {
        // Package-qualified access: package.Symbol
        // Check if the symbol is exported from the package
        if (!isEntryPoint_) {
            // Non-main package can only access non-ECS symbols
            reportError(node.loc, "Non-main package cannot use package-qualified ECS access. Use utils/ functions instead.");
        } else {
            // Main package: check if symbol is exported
            std::string symbolName = node.member;
            
            auto pkgIt = importedPackages_.find(packageName);
            if (pkgIt != importedPackages_.end()) {
                const auto& imported = pkgIt->second;
                bool isValid = imported.functions.count(symbolName) ||
                              imported.externFunctions.count(symbolName) ||
                              imported.externTypes.count(symbolName) ||
                              imported.handles.count(symbolName) ||
                              imported.datas.count(symbolName) ||
                              imported.traits.count(symbolName) ||
                              imported.templates.count(symbolName);
                
                if (!isValid) {
                    reportError(node.loc, "Symbol '" + symbolName + "' is not exported from package '" + packageName + "'");
                }
            }
        }
    } else if (inTraitSystem_) {
        // In trait system, enforce new syntax: entityName.ComponentName.field
        // Handle nested MemberExpr: obj.Component.field
        // First, find the root object (should be entity variable)
        const Expr* root = &node;
        std::vector<std::string> memberChain;
        while (auto* me = dynamic_cast<const MemberExpr*>(root)) {
            memberChain.push_back(std::string(me->member));
            root = me->object.get();
        }
        
        // Check if root is the entity variable
        if (auto* idExpr = dynamic_cast<const IdentifierExpr*>(root)) {
            if (std::string(idExpr->name) == currentEntityVarName_ && !memberChain.empty()) {
                // Reverse to get correct order: [Component, field]
                std::reverse(memberChain.begin(), memberChain.end());
                
                // First member in chain is the component name
                std::string compName = memberChain[0];
                
                // Check if component is available in this system
                // Component's package is determined by the trait's package
                if (!currentSystemComponents_.count(compName)) {
                    reportError(node.loc, "Component '" + compName + "' is not available in this system");
                }
                
                // Valid syntax, process nested members recursively
                if (memberChain.size() > 1) {
                    node.object->accept(*this);
                }
                allowArrayLiteral_ = savedAllowArrayLiteral;
                return;
            } else if (currentSystemComponents_.count(idExpr->name)) {
                // Old syntax: directly using ComponentName - report error
                reportError(node.loc, "Old syntax is no longer supported. Use '" + currentEntityVarName_ + "." + idExpr->name + ".field' instead of '" + idExpr->name + ".field'");
                allowArrayLiteral_ = savedAllowArrayLiteral;
                return;
            }
        }
        node.object->accept(*this);
        } else {
            // Normal member access
            node.object->accept(*this);
        }

    // Restore array literal allowance
    allowArrayLiteral_ = savedAllowArrayLiteral;
}

inline void SemanticAnalyzer::visit(IndexExpr& node) {
    // Index expression operands don't allow array literals
    bool savedAllowArrayLiteral = allowArrayLiteral_;
    allowArrayLiteral_ = false;
    node.object->accept(*this);
    node.index->accept(*this);
    allowArrayLiteral_ = savedAllowArrayLiteral;

    // Check if object is indexable (array or string)
    auto objType = inferTypeKind(*node.object);
    if (objType != TypeKind::String && objType != TypeKind::Array) {
        reportError(node.loc, "Cannot index type '" + typeKindToString(objType) + "'. Only strings and arrays can be indexed.");
    }

    // Check if index is integer type
    auto idxType = inferTypeKind(*node.index);
    if (!isIntegerType(idxType)) {
        reportError(node.loc, "Index must be an integer type, got '" + typeKindToString(idxType) + "'.");
    }
}

inline void SemanticAnalyzer::visit(SpawnExpr& node) {
    // Empty template name means create empty entity (no template)
    if (!node.templateName.empty() && !isTemplateDefined(node.templateName)) {
        reportError(node.loc, "Undefined template: " + node.templateName);
    }
}

inline void SemanticAnalyzer::visit(HasExpr& node) {
    node.entity->accept(*this);

    // Check if it's a trait or component
    if (isTraitDefined(node.name)) {
        node.isTrait = true;
    } else if (isDataDefined(node.name)) {
        node.isTrait = false;
    } else {
        reportError(node.loc, "Undefined trait or component in 'has' check: " + node.name);
        return;
    }
    
    // World check: entity can only check for traits/components from its own world
    // STRICT: Entity can only have traits/components from its own world
    if (isEntryPoint_) {
        std::string itemPkg = node.isTrait ? getTraitPackage(node.name) : getDataPackage(node.name);
        
        // Try to get entity world from variable tracking
        std::string entityWorld = "";
        if (auto* identExpr = dynamic_cast<IdentifierExpr*>(node.entity.get())) {
            entityWorld = getVariableWorld(identExpr->name);
        }
        
        // If entity world is known, trait/component must be from the same world
        if (!entityWorld.empty() && entityWorld != itemPkg) {
            std::string itemType = node.isTrait ? "trait" : "component";
            reportError(node.loc, "World mismatch: Cannot check for " + itemType + " '" + node.name + 
                       "' (from '" + itemPkg + "' world) on entity from '" + entityWorld + "' world. " +
                       "An entity can only have " + itemType + "s from its own world.");
            return;
        }
    }
}

inline void SemanticAnalyzer::visit(ArrayLiteralExpr& node) {
    // Array literals can only be used in variable initialization
    if (!allowArrayLiteral_) {
        reportError(node.loc, "Array literals can only be used in variable initialization. "
                   "Use a variable declaration like 'let arr = [1, 2, 3];' instead.");
    }
    
    // Check all elements and infer array element type
    TypeKind elementType = TypeKind::Void;
    bool firstElement = true;
    
    for (auto& elem : node.elements) {
        elem->accept(*this);
        auto elemType = inferTypeKind(*elem);
        
        if (firstElement) {
            elementType = elemType;
            firstElement = false;
        } else if (elemType != elementType) {
            // Check type compatibility
            if (!isTypeCompatible(elementType, elemType)) {
                reportError(node.loc, "Array elements must have compatible types. Expected " + 
                           typeKindToString(elementType) + ", got " + typeKindToString(elemType));
            }
        }
    }
    
    // Store element type for later use in type inference
    // We use a special key format to track this array literal's element type
    if (!firstElement) {
        // The element type is stored in the AST node implicitly through the first element
        // Code generator will use the first element's type
    }
}

inline void SemanticAnalyzer::visit(BlockStmt& node) {
    enterScope();
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    exitScope();
}

inline void SemanticAnalyzer::visit(VarDeclStmt& node) {
    // Check for shadowing first
    if (isVariableDeclared(node.name)) {
        std::string msg = "Variable '" + node.name + "' shadows existing variable";
        if (node.name == "e") {
            msg += "\n  Note: 'e' is reserved for the current entity in trait systems. Use a different variable name.";
        }
        reportError(node.loc, msg);
    }

    // Declare variable first (before processing init, so recursive references work)
    // Record mutability: var = mutable, let = immutable
    declareVariableWithMutability(node.name, node.isMutable);

    // If there's an explicit type, check it's defined and record it
    if (node.type) {
        if (!isTypeDefined(*node.type)) {
            reportError(node.loc, "Undefined type in variable declaration '" + node.name + 
                       "': " + getTypeName(*node.type));
        }
        // Check for void type variables - void can only be used as function return type
        if (node.type->kind == TypeKind::Void) {
            reportError(node.loc, "Cannot declare variable '" + node.name + 
                       "' with type 'void'. 'void' can only be used as function return type.");
        }
        // Clone the type to preserve world information
        auto typeCopy = std::make_unique<Type>(node.type->kind);
        typeCopy->name = node.type->name;
        typeCopy->world = node.type->world;
        declareVariableWithType(node.name, std::move(typeCopy));
    }

    // Process initialization (allow array literals in this context)
    if (node.init) {
        allowArrayLiteral_ = true;
        node.init->accept(*this);
        allowArrayLiteral_ = false;

        // Check: f-string cannot be stored in a variable
        checkFStringNotStored(*node.init, node.loc, "Cannot initialize variable '" + node.name + "' with f-string:");

        // Infer type from initialization if no explicit type
        if (!node.type) {
            auto initType = inferTypeKind(*node.init);
            // Check for void type inference
            if (initType == TypeKind::Void) {
                reportError(node.loc, "Cannot declare variable '" + node.name + 
                           "' with inferred type 'void'. 'void' can only be used as function return type.");
            }
            // For entity type, try to infer world from spawn expression
            if (initType == TypeKind::Entity) {
                auto spawnExpr = dynamic_cast<SpawnExpr*>(node.init.get());
                if (spawnExpr) {
                    auto entityType = std::make_unique<Type>(TypeKind::Entity);
                    // Empty template name means empty entity, use current package
                    if (!spawnExpr->templateName.empty()) {
                        entityType->world = getTemplatePackage(spawnExpr->templateName);
                    } else {
                        entityType->world = currentPackageName_;
                    }
                    declareVariableWithType(node.name, std::move(entityType));
                } else {
                    declareVariableWithTypeKind(node.name, initType);
                }
            } else {
                declareVariableWithTypeKind(node.name, initType);
            }
        } else {
            // Check type compatibility
            auto initType = inferTypeKind(*node.init);
            checkTypeCompatibility(node.loc, node.type->kind, initType,
                "Variable '" + node.name + "' type mismatch");
        }
    }
}

inline void SemanticAnalyzer::visit(ExprStmt& node) {
    node.expr->accept(*this);
}

inline void SemanticAnalyzer::visit(IfStmt& node) {
    node.condition->accept(*this);
    node.thenBranch->accept(*this);
    if (node.elseBranch) {
        node.elseBranch->accept(*this);
    }
}

inline void SemanticAnalyzer::visit(WhileStmt& node) {
    node.condition->accept(*this);
    node.body->accept(*this);
}



inline void SemanticAnalyzer::visit(ReturnStmt& node) {
    if (node.value) {
        node.value->accept(*this);

        // Check return type compatibility
        auto returnValueType = inferTypeKind(*node.value);
        checkTypeCompatibility(node.loc, currentReturnType_, returnValueType,
            "Return type mismatch");

        // Check: f-string cannot be returned
        checkFStringNotStored(*node.value, node.loc, "Cannot return f-string:");
    } else if (currentReturnType_ != TypeKind::Void) {
        reportError(node.loc, "Function must return a value of type " + typeKindToString(currentReturnType_));
    }
}

inline void SemanticAnalyzer::visit(GiveStmt& node) {
    node.entity->accept(*this);

    if (!isDataDefined(node.componentName)) {
        reportError(node.loc, "Undefined component in 'give': " + node.componentName);
        return;
    }

    // World check: component must belong to the same world as the entity
    // STRICT: Entity can only have components from its own world
    if (isEntryPoint_) {
        std::string compPkg = getDataPackage(node.componentName);
        
        // Try to get entity world from variable tracking
        std::string entityWorld = "";
        if (auto* identExpr = dynamic_cast<IdentifierExpr*>(node.entity.get())) {
            entityWorld = getVariableWorld(identExpr->name);
        }
        
        // If entity world is known, component must be from the same world
        if (!entityWorld.empty() && entityWorld != compPkg) {
            reportError(node.loc, "World mismatch: Cannot give component '" + node.componentName + 
                       "' (from '" + compPkg + "' world) to entity from '" + entityWorld + "' world. " +
                       "An entity can only have components from its own world.");
            return;
        }
    }

    for (auto& field : node.initFields) {
        field.second->accept(*this);
    }
}

inline void SemanticAnalyzer::visit(TagStmt& node) {
    node.entity->accept(*this);

    if (!isTraitDefined(node.traitName)) {
        reportError(node.loc, "Undefined trait in 'tag': " + node.traitName);
        return;
    }

    // World check: trait must belong to the same world as the entity
    // STRICT: Entity can only have traits from its own world
    if (isEntryPoint_) {
        std::string traitPkg = getTraitPackage(node.traitName);
        
        // Try to get entity world from variable tracking
        std::string entityWorld = "";
        if (auto* identExpr = dynamic_cast<IdentifierExpr*>(node.entity.get())) {
            entityWorld = getVariableWorld(identExpr->name);
        }
        
        // If entity world is known, trait must be from the same world
        if (!entityWorld.empty() && entityWorld != traitPkg) {
            reportError(node.loc, "World mismatch: Cannot tag entity from '" + entityWorld + 
                       "' world with trait '" + node.traitName + "' (from '" + traitPkg + "' world). " +
                       "An entity can only have traits from its own world.");
            return;
        }
    }
}

inline void SemanticAnalyzer::visit(UntagStmt& node) {
    node.entity->accept(*this);

    if (!isTraitDefined(node.traitName)) {
        reportError(node.loc, "Undefined trait in 'untag': " + node.traitName);
        return;
    }

    // World check: trait must belong to the same world as the entity
    // STRICT: Entity can only have traits from its own world
    if (isEntryPoint_) {
        std::string traitPkg = getTraitPackage(node.traitName);
        
        // Try to get entity world from variable tracking
        std::string entityWorld = "";
        if (auto* identExpr = dynamic_cast<IdentifierExpr*>(node.entity.get())) {
            entityWorld = getVariableWorld(identExpr->name);
        }
        
        // If entity world is known, trait must be from the same world
        if (!entityWorld.empty() && entityWorld != traitPkg) {
            reportError(node.loc, "World mismatch: Cannot untag entity from '" + entityWorld + 
                       "' world with trait '" + node.traitName + "' (from '" + traitPkg + "' world). " +
                       "An entity can only have traits from its own world.");
            return;
        }
    }
}

inline void SemanticAnalyzer::visit(DestroyStmt& node) {
    if (node.entity) {
        node.entity->accept(*this);
    } else if (!inTraitSystem_) {
        reportError(node.loc, "'destroy' without entity can only be used in trait systems");
    }
}

inline void SemanticAnalyzer::visit(QueryStmt& node) {
    for (const auto& traitName : node.traitNames) {
        if (!isTraitDefined(traitName)) {
            reportError(node.loc, "Undefined trait in 'query': " + traitName);
        }
    }

    for (const auto& compName : node.withComponents) {
        if (!isDataDefined(compName)) {
            reportError(node.loc, "Undefined component in 'query': " + compName);
        }
    }

    // Collect available components from queried trait
    std::unordered_set<std::string> savedComponents = currentSystemComponents_;
    currentSystemComponents_.clear();
    for (const auto& traitName : node.traitNames) {
        auto it = traitComponents_.find(traitName);
        if (it != traitComponents_.end()) {
            for (const auto& comp : it->second) {
                currentSystemComponents_.insert(comp);
            }
        }
    }
    // Add withComponents
    for (const auto& compName : node.withComponents) {
        currentSystemComponents_.insert(compName);
    }

    enterScope();
    // Enable trait system mode with the query's entity variable name
    bool savedInTraitSystem = inTraitSystem_;
    std::string savedEntityVar = currentEntityVarName_;
    inTraitSystem_ = true;
    currentEntityVarName_ = node.varName;
    // Declare entity variable (for 'as' clause) - will error if body tries to redeclare it
    declareVariable(node.varName);
    node.body->accept(*this);
    inTraitSystem_ = savedInTraitSystem;
    currentEntityVarName_ = savedEntityVar;
    currentSystemComponents_ = savedComponents;
    exitScope();
}

inline void SemanticAnalyzer::visit(LockStmt& node) {
    // Lock can operate on:
    // - Main package: any system's full path (package.system)
    // - Non-main package: only local systems (single name, no package prefix)
    if (node.systemPath.size() > 1) {
        // Cross-package system reference
        if (!isEntryPoint_) {
            // Non-main package cannot access other packages' systems
            std::string fullPath;
            for (size_t i = 0; i < node.systemPath.size(); i++) {
                if (i > 0) fullPath += ".";
                fullPath += node.systemPath[i];
            }
            reportError(node.loc, 
                "Cannot lock system from another package: '" + fullPath + "'. " +
                "Only main package can lock/unlock systems from other packages.");
        }
        // Main package: allow cross-package lock, code generator will handle the world
    } else if (node.systemPath.size() == 1) {
        // Single name - must be local system
        const std::string& sysName = node.systemPath[0];
        if (localSystemNames_.find(sysName) == localSystemNames_.end()) {
            reportError(node.loc,
                "Cannot lock system '" + sysName + "'. " +
                "System must be defined in the current package to be locked/unlocked.");
        }
    }
}

inline void SemanticAnalyzer::visit(UnlockStmt& node) {
    // Unlock can operate on:
    // - Main package: any system's full path (package.system)
    // - Non-main package: only local systems (single name, no package prefix)
    if (node.systemPath.size() > 1) {
        // Cross-package system reference
        if (!isEntryPoint_) {
            // Non-main package cannot access other packages' systems
            std::string fullPath;
            for (size_t i = 0; i < node.systemPath.size(); i++) {
                if (i > 0) fullPath += ".";
                fullPath += node.systemPath[i];
            }
            reportError(node.loc,
                "Cannot unlock system from another package: '" + fullPath + "'. " +
                "Only main package can lock/unlock systems from other packages.");
        }
        // Main package: allow cross-package unlock, code generator will handle the world
    } else if (node.systemPath.size() == 1) {
        // Single name - must be local system
        const std::string& sysName = node.systemPath[0];
        if (localSystemNames_.find(sysName) == localSystemNames_.end()) {
            reportError(node.loc,
                "Cannot unlock system '" + sysName + "'. " +
                "System must be defined in the current package to be locked/unlocked.");
        }
    }
}

inline void SemanticAnalyzer::visit(DataDecl& node) {
    // Check field types and calculate component size
    size_t totalSize = 0;
    for (const auto& field : node.fields) {
        if (field.defaultValue) {
            field.defaultValue->accept(*this);
        }
        
        // ❌ Ban entity type in components (breaks cache coherence and causes pointer invalidation)
        if (field.type && field.type->kind == TypeKind::Entity) {
            reportError(node.loc, 
                "Component '" + node.name + "' field '" + field.name + 
                "': entity type is not allowed in components.\n" +
                "Reasons:\n" +
                "  1. Entity references in components break cache-friendly SoA layout\n" +
                "  2. Entity handles become invalid when entities move between archetypes\n" +
                "  3. Use entity ID (u64) instead, or store relationships in separate components\n" +
                "Alternative: Use u64 to store entity index if cross-entity reference is needed.");
        }
        
        // ❌ Also ban arrays of entities
        if (field.type && field.type->kind == TypeKind::Array && 
            field.type->base && field.type->base->kind == TypeKind::Entity) {
            reportError(node.loc,
                "Component '" + node.name + "' field '" + field.name +
                "': arrays of entity are not allowed in components.\n" +
                "Use arrays of u64 (entity IDs) instead.");
        }
        
        // Check field type is defined (for UserDefined types)
        if (field.type && !isTypeDefined(*field.type)) {
            reportError(node.loc, "Component '" + node.name + "' field '" + field.name + 
                       "': undefined type '" + getTypeName(*field.type) + "'");
        }
        
        // Calculate field size
        size_t fieldSize = 0;
        switch (field.type->kind) {
            case TypeKind::I8:
            case TypeKind::U8:
            case TypeKind::Bool:
                fieldSize = 1;
                break;
            case TypeKind::I16:
            case TypeKind::U16:
                fieldSize = 2;
                break;
            case TypeKind::I32:
            case TypeKind::U32:
            case TypeKind::F32:
                fieldSize = 4;
                break;
            case TypeKind::I64:
            case TypeKind::U64:
            case TypeKind::F64:
            case TypeKind::Entity:
            case TypeKind::String:
            case TypeKind::Pointer:
                fieldSize = 8;
                break;
            case TypeKind::Array:
                // Calculate array size: base type size * array size
                {
                    size_t elemSize = 0;
                    switch (field.type->base->kind) {
                        case TypeKind::I8:
                        case TypeKind::U8:
                        case TypeKind::Bool:
                            elemSize = 1;
                            break;
                        case TypeKind::I16:
                        case TypeKind::U16:
                            elemSize = 2;
                            break;
                        case TypeKind::I32:
                        case TypeKind::U32:
                        case TypeKind::F32:
                            elemSize = 4;
                            break;
                        case TypeKind::I64:
                        case TypeKind::U64:
                        case TypeKind::F64:
                        case TypeKind::Entity:
                        case TypeKind::String:
                        case TypeKind::Pointer:
                            elemSize = 8;
                            break;
                        default:
                            elemSize = 4;
                    }
                    fieldSize = elemSize * field.type->arraySize;
                }
                break;
            case TypeKind::UserDefined:
                // For user-defined types, we can't easily know the size here
                // Assume 64 bytes as a conservative estimate
                fieldSize = 64;
                break;
            default:
                fieldSize = 4;
        }
        totalSize += fieldSize;
        
        // Store field type for later type inference (e.g., array indexing)
        std::string fieldKey = node.name + "." + field.name;
        componentFieldTypes_[fieldKey] = field.type->kind;
        
        // For arrays, also store the element type
        if (field.type->kind == TypeKind::Array && field.type->base) {
            arrayElementTypes_[fieldKey] = field.type->base->kind;
        }
    }
    
    // Warn if component size exceeds cache line (64 bytes)
    if (totalSize > 64) {
        reportWarning(node.loc, "Component '" + node.name + "' size (" + 
                     std::to_string(totalSize) + " bytes) exceeds 64 bytes (cache line). " +
                     "Consider splitting it into smaller components for better cache performance.");
    }
}

inline void SemanticAnalyzer::visit(TraitDecl& node) {
    // Check that all required components exist
    for (const auto& compName : node.requiredData) {
        if (!isDataDefined(compName)) {
            reportError(node.loc, "Undefined component in trait '" + node.name + "': " + compName);
        }
    }
}

inline void SemanticAnalyzer::visit(TemplateDecl& node) {
    // Check component initializations
    for (const auto& comp : node.components) {
        if (!isDataDefined(comp.componentName)) {
            reportError(node.loc, "Undefined component in template '" + node.name + "': " + comp.componentName);
        }
        for (const auto& field : comp.fieldValues) {
            field.second->accept(*this);
        }
    }

    // Check tags
    for (const auto& tag : node.tags) {
        if (!isTraitDefined(tag)) {
            reportError(node.loc, "Undefined trait in template '" + node.name + "': " + tag);
        }
    }
}

inline void SemanticAnalyzer::visit(SystemDecl& node) {
    // Check trait existence and locality
    std::vector<std::string> traitWorlds;
    for (const auto& fullTraitName : node.forTraits) {
        // Parse trait name - must contain package prefix for cross-package access
        std::string traitName = fullTraitName;
        std::string traitPkg = currentPackageName_;
        size_t sepPos = traitName.find("__");
        if (sepPos != std::string::npos) {
            traitPkg = traitName.substr(0, sepPos);
            traitName = traitName.substr(sepPos + 2);
        } else if (isEntryPoint_) {
            // Main package: trait without package prefix - must be local
            if (!isTraitDefinedLocally(traitName)) {
                // Not local, must be from imported package - require explicit prefix
                reportError(node.loc, 
                    "Cannot use trait '" + fullTraitName + "' without package prefix. " +
                    "Use 'package.Trait' syntax to explicitly specify which package the trait belongs to. " +
                    "Available packages: current package '" + currentPackageName_ + "', imported packages: " +
                    [&]() {
                        std::string pkgs;
                        for (const auto& [name, _] : importedPackages_) {
                            if (!pkgs.empty()) pkgs += ", ";
                            pkgs += name;
                        }
                        return pkgs.empty() ? "none" : pkgs;
                    }());
                continue;
            }
        }
        
        bool traitExists = isTraitDefined(traitName);
        // Also check if it's an imported trait from the specified package
        if (!traitExists && traitPkg != currentPackageName_) {
            auto pkgIt = importedPackages_.find(traitPkg);
            if (pkgIt != importedPackages_.end() && pkgIt->second.traits.count(traitName)) {
                traitExists = true;
            }
        }
        
        if (!traitExists) {
            reportError(node.loc, "Undefined trait in system '" + node.name + "': " + fullTraitName);
        } else if (!isEntryPoint_ && traitPkg != currentPackageName_) {
            // Non-main packages: Trait must be defined in the current package
            reportError(node.loc,
                "Cannot use trait '" + fullTraitName + "' from another package in system '" + node.name + "'. " +
                "Trait systems can only use traits defined in the current package.");
        }
        
        // Collect trait worlds for consistency check
        if (isEntryPoint_) {
            traitWorlds.push_back(traitPkg);
        }
    }
    
    // Check trait world consistency in entry point
    if (isEntryPoint_ && !traitWorlds.empty()) {
        checkWorldConsistency(node.loc, traitWorlds, "system '" + node.name + "'");
    }

    // Collect available components from traits
    currentSystemComponents_.clear();
    for (const auto& fullTraitName : node.forTraits) {
        // Parse trait name
        std::string traitName = fullTraitName;
        std::string traitPkg = currentPackageName_;
        size_t sepPos = traitName.find("__");
        if (sepPos != std::string::npos) {
            traitPkg = traitName.substr(0, sepPos);
            traitName = traitName.substr(sepPos + 2);
        }
        
        // Check local traits first
        auto it = traitComponents_.find(traitName);
        if (it != traitComponents_.end()) {
            for (const auto& comp : it->second) {
                currentSystemComponents_.insert(comp);
            }
        }
        
        // For main package, also check imported traits
        if (isEntryPoint_) {
            for (const auto& [pkgName, imported] : importedPackages_) {
                if (pkgName == traitPkg && imported.traits.count(traitName)) {
                    // Get components from PackageManager
                    PackageInfo* pkgInfo = packageManager.loadPackageForUse({pkgName}, false);
                    if (pkgInfo && pkgInfo->ast) {
                        for (const auto& trait : pkgInfo->ast->traits) {
                            if (trait->name == traitName) {
                                for (const auto& comp : trait->requiredData) {
                                    currentSystemComponents_.insert(comp);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Check #batch attribute
    if (node.isBatch) {
        std::string reason;
        if (!canVectorizeSystem(node, reason)) {
            reportWarning(node.loc, "System '" + node.name + "' is marked with #batch but cannot be vectorized: " + reason);
        }
    }

    // Check body
    enterScope();
    inTraitSystem_ = !node.forTraits.empty();
    currentEntityVarName_ = node.entityVarName;  // Set the entity variable name

    // Declare entity variable (for 'as' clause) before checking body
    // This will cause an error if body tries to redeclare it
    declareVariable(currentEntityVarName_);

    // Declare system parameters
    for (const auto& param : node.params) {
        declareVariable(param.name);
    }

    if (node.body) {
        node.body->accept(*this);
    }

    inTraitSystem_ = false;
    currentEntityVarName_.clear();
    currentSystemComponents_.clear();
    exitScope();
}

inline void SemanticAnalyzer::visit(ExternTypeDecl& node) {
    // Extern types are already registered in collectDeclarations()
    // This pass only handles export declarations
}

inline void SemanticAnalyzer::visit(HandleDecl& node) {
    // Check for duplicate handle type definition
    if (handleNames_.count(node.name)) {
        reportError(node.loc, "Duplicate handle type definition: " + node.name);
    } else if (externTypeNames_.count(node.name) || dataNames_.count(node.name)) {
        reportError(node.loc, "Handle type name conflicts with existing type: " + node.name);
    } else {
        handleNames_.insert(node.name);
        handleDtors_[node.name] = node.dtor;
    }
    
    // Check that destructor function is declared
    if (!node.dtor.empty()) {
        if (!externFunctionNames_.count(node.dtor)) {
            reportError(node.loc, "Handle destructor function '" + node.dtor + "' is not declared. "
                       "Use 'extern fn " + node.dtor + "(...);' to declare it.");
        } else {
            // Check destructor signature: must return void and have exactly 1 parameter
            auto retIt = functionReturnTypes_.find(node.dtor);
            auto paramIt = functionParamTypes_.find(node.dtor);
            
            if (retIt != functionReturnTypes_.end() && retIt->second != TypeKind::Void) {
                reportError(node.loc, "Handle destructor function '" + node.dtor + "' must return 'void'.");
            }
            
            if (paramIt != functionParamTypes_.end() && paramIt->second.size() != 1) {
                reportError(node.loc, "Handle destructor function '" + node.dtor + "' must have exactly 1 parameter.");
            }
        }
    }
}

inline void SemanticAnalyzer::visit(ExportDecl& node) {
    // Determine if it's a type or function based on what we find
    bool isExternType = externTypeNames_.count(node.name);
    bool isExternFunction = externFunctionNames_.count(node.name);
    
    if (isExternType) {
        node.isType = true;
        exportedExternTypes_.insert(node.name);
    } else if (isExternFunction) {
        node.isType = false;
        exportedExternFunctions_.insert(node.name);
    } else {
        reportError(node.loc, "Cannot export '" + node.name + "': not an extern type or function");
    }
}

inline void SemanticAnalyzer::visit(DependencyDecl& node) {
    // Check all systems in the chain are defined
    for (const auto& sysName : node.systems) {
        if (!isSystemDefined(sysName)) {
            reportError(node.loc, "Undefined system in dependency: " + sysName);
        }
    }
}

inline void SemanticAnalyzer::visit(FnDecl& node) {
    // Check parameter types are defined
    for (const auto& param : node.params) {
        if (param.type && !isTypeDefined(*param.type)) {
            reportError(node.loc, "Undefined type in function parameter '" + param.name + "': " + getTypeName(*param.type));
        }
    }
    
    // Check return type is defined
    if (node.returnType && !isTypeDefined(*node.returnType)) {
        reportError(node.loc, "Undefined return type in function: " + getTypeName(*node.returnType));
    }
    
    enterScope();

    // Set current return type
    if (node.returnType) {
        currentReturnType_ = node.returnType->kind;
    } else {
        currentReturnType_ = TypeKind::Void;
    }

    // Declare parameters with types
    for (const auto& param : node.params) {
        declareVariable(param.name);
        if (param.type) {
            declareVariableWithTypeKind(param.name, param.type->kind);
        }
    }

    if (node.body) {
        node.body->accept(*this);
    }

    currentReturnType_ = TypeKind::Void;
    exitScope();
}

inline void SemanticAnalyzer::visit(Package& node) {
    // Package-level analysis is handled in analyze()
}

// ============================================================================
// Type Checking Implementations
// ============================================================================

inline TypeKind SemanticAnalyzer::inferTypeKind(Expr& expr) {
    if (auto* intExpr = dynamic_cast<IntegerExpr*>(&expr)) {
        (void)intExpr;
        return TypeKind::I32;
    } else if (auto* floatExpr = dynamic_cast<FloatExpr*>(&expr)) {
        (void)floatExpr;
        return TypeKind::F32;
    } else if (auto* boolExpr = dynamic_cast<BoolExpr*>(&expr)) {
        (void)boolExpr;
        return TypeKind::Bool;
    } else if (auto* stringExpr = dynamic_cast<StringExpr*>(&expr)) {
        (void)stringExpr;
        return TypeKind::String;
    } else if (auto* fstringExpr = dynamic_cast<FStringExpr*>(&expr)) {
        (void)fstringExpr;
        return TypeKind::String;
    } else if (auto* idExpr = dynamic_cast<IdentifierExpr*>(&expr)) {
        TypeKind varType = getVariableTypeKind(idExpr->name);
        // If variable type is not found, it might be a forward reference
        // or an undefined variable (which will be caught by visit(IdentifierExpr))
        // Return I32 as a safe default to avoid cascading type errors
        if (varType == TypeKind::Void) {
            return TypeKind::I32;  // Default assumption for unresolved identifiers
        }
        return varType;
    } else if (auto* binaryExpr = dynamic_cast<BinaryExpr*>(&expr)) {
        auto leftType = inferTypeKind(*binaryExpr->left);
        auto rightType = inferTypeKind(*binaryExpr->right);

        switch (binaryExpr->op) {
            case BinOp::Add:
            case BinOp::Sub:
            case BinOp::Mul:
            case BinOp::Div:
            case BinOp::Mod:
                // Arithmetic operations result in the wider type
                if (leftType == TypeKind::F64 || rightType == TypeKind::F64) {
                    return TypeKind::F64;
                } else if (leftType == TypeKind::F32 || rightType == TypeKind::F32) {
                    return TypeKind::F32;
                } else if (leftType == TypeKind::I64 || rightType == TypeKind::I64) {
                    return TypeKind::I64;
                } else {
                    return TypeKind::I32;
                }
            case BinOp::Eq:
            case BinOp::Ne:
            case BinOp::Lt:
            case BinOp::Gt:
            case BinOp::Le:
            case BinOp::Ge:
                // Comparison operations result in bool
                return TypeKind::Bool;
            case BinOp::And:
            case BinOp::Or:
                // Logical operations result in bool
                return TypeKind::Bool;
            case BinOp::Assign:
            case BinOp::AddAssign:
            case BinOp::SubAssign:
                // Assignment returns the left type
                return leftType;
        }
    } else if (auto* unaryExpr = dynamic_cast<UnaryExpr*>(&expr)) {
        auto operandType = inferTypeKind(*unaryExpr->operand);
        switch (unaryExpr->op) {
            case UnOp::Neg:
                return operandType;
            case UnOp::Not:
                return TypeKind::Bool;
            case UnOp::Deref:
            case UnOp::AddrOf:
                return operandType;  // Simplified
        }
    } else if (auto* spawnExpr = dynamic_cast<SpawnExpr*>(&expr)) {
        (void)spawnExpr;
        return TypeKind::Entity;
    } else if (auto* callExpr = dynamic_cast<CallExpr*>(&expr)) {
        // Look up function return type
        if (auto* idExpr = dynamic_cast<IdentifierExpr*>(callExpr->callee.get())) {
            // Direct function call: funcName()
            auto it = functionReturnTypes_.find(idExpr->name);
            if (it != functionReturnTypes_.end()) {
                return it->second;
            }
        } else if (auto* memberExpr = dynamic_cast<MemberExpr*>(callExpr->callee.get())) {
            // Package-qualified call: package.funcName()
            if (auto* pkgId = dynamic_cast<IdentifierExpr*>(memberExpr->object.get())) {
                std::string pkgName = pkgId->name;
                std::string funcName = memberExpr->member;
                // Resolve alias if present
                auto aliasIt = packageAliases_.find(pkgName);
                if (aliasIt != packageAliases_.end()) {
                    pkgName = aliasIt->second;
                }
                std::string fullFuncName = pkgName + "__" + funcName;
                auto it = functionReturnTypes_.find(fullFuncName);
                if (it != functionReturnTypes_.end()) {
                    return it->second;
                }
            }
        }
        return TypeKind::Void;
    } else if (auto* hasExpr = dynamic_cast<HasExpr*>(&expr)) {
        (void)hasExpr;
        return TypeKind::Bool;
    } else if (auto* idxExpr = dynamic_cast<IndexExpr*>(&expr)) {
        // Index expression: return element type
        // For MemberExpr like entity.Component.field[index], we need to look up field type
        TypeKind objType = TypeKind::Void;
        std::string fieldKey; // For looking up array element type
        
        // Try to get the object type and field key for array element lookup
        if (auto* memberExpr = dynamic_cast<MemberExpr*>(idxExpr->object.get())) {
            // Check if it's entity.Component.field pattern
            if (auto* innerMember = dynamic_cast<MemberExpr*>(memberExpr->object.get())) {
                if (auto* idExpr = dynamic_cast<IdentifierExpr*>(innerMember->object.get())) {
                    std::string compName = innerMember->member;
                    std::string fieldName = memberExpr->member;
                    fieldKey = compName + "." + fieldName;
                    auto it = componentFieldTypes_.find(fieldKey);
                    if (it != componentFieldTypes_.end()) {
                        objType = it->second;
                    }
                }
            } else if (auto* idExpr = dynamic_cast<IdentifierExpr*>(memberExpr->object.get())) {
                // Direct Component.field pattern
                std::string compName = idExpr->name;
                std::string fieldName = memberExpr->member;
                fieldKey = compName + "." + fieldName;
                auto it = componentFieldTypes_.find(fieldKey);
                if (it != componentFieldTypes_.end()) {
                    objType = it->second;
                }
            }
        } else {
            objType = inferTypeKind(*idxExpr->object);
        }
        
        if (objType == TypeKind::String) {
            return TypeKind::U8;  // String[index] returns char (u8, not i8 as it should be unsigned)
        } else if (objType == TypeKind::Array && !fieldKey.empty()) {
            // Look up actual element type from arrayElementTypes_
            auto it = arrayElementTypes_.find(fieldKey);
            if (it != arrayElementTypes_.end()) {
                return it->second;
            }
            return TypeKind::I32;  // Fallback
        } else if (objType == TypeKind::Array) {
            // For regular array variables, infer from the variable's element type
            // Try to get element type from the array variable
            if (auto* idExpr = dynamic_cast<IdentifierExpr*>(idxExpr->object.get())) {
                // Look up variable type in all scopes (from inner to outer)
                for (auto it = varTypes_.rbegin(); it != varTypes_.rend(); ++it) {
                    auto varIt = it->find(idExpr->name);
                    if (varIt != it->end() && varIt->second->kind == TypeKind::Array && varIt->second->base) {
                        return varIt->second->base->kind;
                    }
                }
            }
            return TypeKind::I32;  // Default fallback for arrays
        }
        return TypeKind::Void;
    } else if (auto* arrLitExpr = dynamic_cast<ArrayLiteralExpr*>(&expr)) {
        // Array literal expression
        // Infer element type from first element
        if (arrLitExpr->elements.empty()) {
            return TypeKind::Array;  // Empty array - type will be inferred from context
        }
        auto firstElemType = inferTypeKind(*arrLitExpr->elements[0]);
        // Return Array type - element type can be queried separately
        return TypeKind::Array;
    } else if (auto* memberExpr = dynamic_cast<MemberExpr*>(&expr)) {
        // Check if this is a package-qualified function reference (e.g., engine.clamp)
        // Build the full function name path
        std::vector<std::string> pathParts;
        const Expr* current = &expr;
        while (auto* me = dynamic_cast<const MemberExpr*>(current)) {
            pathParts.push_back(me->member);
            current = me->object.get();
        }
        if (auto* idExpr = dynamic_cast<const IdentifierExpr*>(current)) {
            pathParts.push_back(idExpr->name);
        }
        std::reverse(pathParts.begin(), pathParts.end());
        
        // If we have at least 2 parts, it might be a package-qualified function
        if (pathParts.size() >= 2) {
            std::string fullFuncName = pathParts[0];
            for (size_t i = 1; i < pathParts.size(); i++) {
                fullFuncName += "__" + pathParts[i];
            }
            // Look up the function return type
            auto it = functionReturnTypes_.find(fullFuncName);
            if (it != functionReturnTypes_.end()) {
                return it->second;
            }
        }
        
        // Otherwise, try to find component field type
        // Support new syntax: entityName.ComponentName.field
        // Try to look up actual field type from componentFieldTypes_
        if (auto* innerMember = dynamic_cast<MemberExpr*>(memberExpr->object.get())) {
            // Nested member access: entity.Component.field or Component.field
            if (auto* idExpr = dynamic_cast<IdentifierExpr*>(innerMember->object.get())) {
                std::string compName = innerMember->member;
                std::string fieldName = memberExpr->member;
                std::string key = compName + "." + fieldName;
                auto it = componentFieldTypes_.find(key);
                if (it != componentFieldTypes_.end()) {
                    return it->second;
                }
            }
        } else if (auto* idExpr = dynamic_cast<IdentifierExpr*>(memberExpr->object.get())) {
            // Direct Component.field access
            std::string compName = idExpr->name;
            std::string fieldName = memberExpr->member;
            std::string key = compName + "." + fieldName;
            auto it = componentFieldTypes_.find(key);
            if (it != componentFieldTypes_.end()) {
                return it->second;
            }
        }
        // Fallback to f32 if field type not found
        return TypeKind::F32;
    }

    return TypeKind::Void;
}

inline TypeKind SemanticAnalyzer::getVariableTypeKind(const std::string& name) {
    // Note: We used to return Entity for implicit 'e' variable, but now entity variable names
    // are explicit and have their types stored in varTypes_.

    // Check scopes from inner to outer
    for (auto it = varTypes_.rbegin(); it != varTypes_.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            if (varIt->second) {
                return varIt->second->kind;
            }
        }
    }

    // Variable not found in type table - return I32 as default to avoid cascading errors
    // (undefined variable error will be reported separately)
    return TypeKind::I32;
}

inline void SemanticAnalyzer::declareVariableWithType(const std::string& name, TypePtr type) {
    if (!varTypes_.empty()) {
        varTypes_.back()[name] = std::move(type);
    }
}

inline void SemanticAnalyzer::declareVariableWithTypeKind(const std::string& name, TypeKind kind) {
    auto type = std::make_unique<Type>(kind);
    declareVariableWithType(name, std::move(type));
}

inline std::string SemanticAnalyzer::getVariableWorld(const std::string& name) {
    for (auto it = varTypes_.rbegin(); it != varTypes_.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            if (varIt->second && varIt->second->kind == TypeKind::Entity) {
                return varIt->second->world;
            }
            return "";
        }
    }
    return "";
}

inline bool SemanticAnalyzer::isTypeCompatible(TypeKind expected, TypeKind actual) {
    if (expected == TypeKind::Void || actual == TypeKind::Void) return true;

    // Same type is always compatible
    if (expected == actual) return true;

    // Numeric type conversions
    if (isNumericType(expected) && isNumericType(actual)) {
        return true;
    }

    return false;
}

inline bool SemanticAnalyzer::isNumericType(TypeKind kind) {
    switch (kind) {
        case TypeKind::I8:
        case TypeKind::I16:
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::U8:
        case TypeKind::U16:
        case TypeKind::U32:
        case TypeKind::U64:
        case TypeKind::F32:
        case TypeKind::F64:
            return true;
        default:
            return false;
    }
}

inline bool SemanticAnalyzer::isIntegerType(TypeKind kind) {
    switch (kind) {
        case TypeKind::I8:
        case TypeKind::I16:
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::U8:
        case TypeKind::U16:
        case TypeKind::U32:
        case TypeKind::U64:
            return true;
        default:
            return false;
    }
}

inline std::string SemanticAnalyzer::typeKindToString(TypeKind kind) {
    switch (kind) {
        case TypeKind::Void: return "void";
        case TypeKind::Bool: return "bool";
        case TypeKind::I8: return "i8";
        case TypeKind::I16: return "i16";
        case TypeKind::I32: return "i32";
        case TypeKind::I64: return "i64";
        case TypeKind::U8: return "u8";
        case TypeKind::U16: return "u16";
        case TypeKind::U32: return "u32";
        case TypeKind::U64: return "u64";
        case TypeKind::F32: return "f32";
        case TypeKind::F64: return "f64";
        case TypeKind::String: return "string";
        case TypeKind::Entity: return "entity";
        case TypeKind::Pointer: return "pointer";
        case TypeKind::Array: return "array";
        case TypeKind::UserDefined: return "user_defined";
        case TypeKind::Function: return "function";
        default: return "unknown";
    }
}

inline void SemanticAnalyzer::checkTypeCompatibility(const Location& loc, TypeKind expected, TypeKind actual, const std::string& context) {
    if (!isTypeCompatible(expected, actual)) {
        reportError(loc, context + ": expected " + typeKindToString(expected) + ", got " + typeKindToString(actual));
    }
}

// Forward declaration for expression checker
inline bool checkExprForBatchViolations(const Expr& expr, std::string& reason);

// Check if a system can be vectorized (SoA optimized)
// Returns true if the system can use SoA API, false otherwise
// If false, 'reason' contains the explanation
inline bool SemanticAnalyzer::canVectorizeSystem(const SystemDecl& node, std::string& reason) {
    // Must be a trait system
    if (node.forTraits.empty()) {
        reason = "not a trait system (regular systems cannot be vectorized)";
        return false;
    }
    
    // Must have a body
    if (!node.body) {
        reason = "system has no body";
        return false;
    }
    
    // Check all statements in the body
    for (const auto& stmt : node.body->statements) {
        // Check for destroy statement
        if (dynamic_cast<DestroyStmt*>(stmt.get())) {
            reason = "contains 'destroy' statement (entity mutation not allowed in batch mode)";
            return false;
        }
        
        // Check for tag statement (entity mutation)
        if (dynamic_cast<TagStmt*>(stmt.get())) {
            reason = "contains 'tag' statement (entity mutation not allowed in batch mode)";
            return false;
        }
        
        // Check for untag statement (entity mutation)
        if (dynamic_cast<UntagStmt*>(stmt.get())) {
            reason = "contains 'untag' statement (entity mutation not allowed in batch mode)";
            return false;
        }
        
        // Check for give statement (entity mutation)
        if (dynamic_cast<GiveStmt*>(stmt.get())) {
            reason = "contains 'give' statement (entity mutation not allowed in batch mode)";
            return false;
        }
        
        // Check for query statement (nested query not allowed)
        if (dynamic_cast<QueryStmt*>(stmt.get())) {
            reason = "contains 'query' statement (nested queries not allowed in batch mode)";
            return false;
        }
        
        // Check for if statement
        if (dynamic_cast<IfStmt*>(stmt.get())) {
            reason = "contains 'if' statement (branching prevents vectorization)";
            return false;
        }
        
        // Check for while loop
        if (dynamic_cast<WhileStmt*>(stmt.get())) {
            reason = "contains 'while' loop (looping prevents vectorization)";
            return false;
        }
        
        // Check for break/continue
        if (dynamic_cast<BreakStmt*>(stmt.get()) || dynamic_cast<ContinueStmt*>(stmt.get())) {
            reason = "contains 'break' or 'continue' (control flow prevents vectorization)";
            return false;
        }
        
        // Check for return statement
        if (dynamic_cast<ReturnStmt*>(stmt.get())) {
            reason = "contains 'return' statement";
            return false;
        }
        
        // Check for lock/unlock
        if (dynamic_cast<LockStmt*>(stmt.get()) || dynamic_cast<UnlockStmt*>(stmt.get())) {
            reason = "contains 'lock' or 'unlock' statement";
            return false;
        }
        
        // Check for exit
        if (dynamic_cast<ExitStmt*>(stmt.get())) {
            reason = "contains 'exit' statement";
            return false;
        }
        
        // Check expression statements for violations
        if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get())) {
            if (!checkExprForBatchViolations(*exprStmt->expr, reason)) {
                return false;
            }
        }
        
        // Check variable declarations for violations in initializer
        if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt.get())) {
            if (varDecl->init) {
                if (!checkExprForBatchViolations(*varDecl->init, reason)) {
                    return false;
                }
            }
        }
    }
    
    // All checks passed
    return true;
}

// Helper function to check expressions for batch/SOA violations
// Recursively checks for: spawn, function calls, 'e' variable access, has checks
inline bool checkExprForBatchViolations(const Expr& expr, std::string& reason) {
    // Check for spawn expression (entity creation)
    if (dynamic_cast<const SpawnExpr*>(&expr)) {
        reason = "contains 'spawn' expression (entity creation not allowed in batch mode)";
        return false;
    }
    
    // Check for function calls
    if (auto* callExpr = dynamic_cast<const CallExpr*>(&expr)) {
        reason = "contains function call (external function calls prevent vectorization)";
        return false;
    }
    
    // Note: We used to check for 'e' variable here, but now entity variable names
    // are customizable (e.g., 'player', 'entity'). The MemberExpr check handles
    // entity.Component access properly for batch mode validation.
    
    // Check for has expression (component/trait check - may introduce branching)
    if (auto* hasExpr = dynamic_cast<const HasExpr*>(&expr)) {
        reason = "contains 'has' check (conditional checks not allowed in batch mode)";
        return false;
    }
    
    // Recursively check binary expressions
    if (auto* binExpr = dynamic_cast<const BinaryExpr*>(&expr)) {
        if (!checkExprForBatchViolations(*binExpr->left, reason)) {
            return false;
        }
        if (!checkExprForBatchViolations(*binExpr->right, reason)) {
            return false;
        }
    }
    
    // Recursively check unary expressions
    if (auto* unExpr = dynamic_cast<const UnaryExpr*>(&expr)) {
        if (!checkExprForBatchViolations(*unExpr->operand, reason)) {
            return false;
        }
    }
    
    // Recursively check member expressions
    if (auto* memExpr = dynamic_cast<const MemberExpr*>(&expr)) {
        if (!checkExprForBatchViolations(*memExpr->object, reason)) {
            return false;
        }
    }
    
    // Recursively check index expressions
    if (auto* idxExpr = dynamic_cast<const IndexExpr*>(&expr)) {
        if (!checkExprForBatchViolations(*idxExpr->object, reason)) {
            return false;
        }
        if (!checkExprForBatchViolations(*idxExpr->index, reason)) {
            return false;
        }
    }
    
    // All checks passed
    return true;
}

} // namespace paani

#endif // PAANI_SEMA_HPP

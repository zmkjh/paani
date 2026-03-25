// CodeGenerator.inl - Code Generator Implementation

#pragma once
#include "expr/FStringGenerator.hpp"
#include "core/Naming.inl"

namespace paani {

// Main generation entry points
inline std::string CodeGenerator::generateHeader(const Package& pkg) {
    setupPackage(pkg);
    initializeGenerators();
    
    ctx_.writeHeaderLn("#ifndef PAANI_GENERATED_" + packageName_ + "_H");
    ctx_.writeHeaderLn("#define PAANI_GENERATED_" + packageName_ + "_H\n");
    ctx_.writeHeaderLn("#include \"paani_ecs.h\"\n");
    
    // Forward declarations
    for (const auto& handle : pkg.handles) {
        ctx_.writeHeaderLn("// Handle: " + handle->name);
    }
    ctx_.writeHeaderLn("");
    
    // Extern types
    for (const auto& extType : pkg.externTypes) {
        ctx_.writeHeaderLn("// Extern type: " + extType->name + " (" + std::to_string(extType->size) + " bytes)");
        ctx_.writeHeaderLn("typedef struct { uint8_t _[" + std::to_string(extType->size) + "]; } " + 
                         Naming::externType(packageName_, extType->name) + ";");
    }
    if (!pkg.externTypes.empty()) ctx_.writeHeaderLn("");
    
    for (const auto& data : pkg.datas) {
        genDataDecl(*data);
    }
    
    for (const auto& trait : pkg.traits) {
        genTraitDecl(*trait);
    }
    
    for (const auto& fn : pkg.functions) {
        genFnDecl(*fn, true);
    }
    
    // Template spawn function declarations
    for (const auto& tmpl : pkg.templates) {
        ctx_.writeHeaderLn("paani_entity_t " + Naming::templateSpawn(packageName_, tmpl->name) + "(paani_world_t* __paani_gen_world);");
    }
    
    // System function declarations (for external benchmarking)
    for (const auto& sys : pkg.systems) {
        std::string sysFuncName = Naming::systemFunction(packageName_, sys->name);
        ctx_.writeHeaderLn("void " + sysFuncName + "(paani_world_t* __paani_gen_world, float dt, void* userdata);");
    }
    if (!pkg.systems.empty()) ctx_.writeHeaderLn("");
    
    // Module init function declaration
    ctx_.writeHeaderLn("void " + Naming::moduleInit(packageName_) + "(paani_world_t* __paani_gen_world);");
    
    ctx_.writeHeaderLn("\n#endif");
    return ctx_.header.str();
}

inline std::string CodeGenerator::generateImpl(const Package& pkg) {
    // Include own header first
    ctx_.writeImplLn("#include \"" + packageName_ + ".h\"");
    
    // Include headers for used packages (for entry point)
    if (pkg.isEntryPoint) {
        for (const auto& use : pkg.uses) {
            std::string pkgName;
            for (size_t i = 0; i < use->packagePath.size(); i++) {
                if (i > 0) pkgName += "__";
                pkgName += std::string(use->packagePath[i]);
            }
            ctx_.writeImplLn("#include \"" + pkgName + ".h\"");
        }
    }
    
    ctx_.writeImplLn("#include \"paani_ecs.h\"\n");

    // Build component field types map (needed for f-string type inference)
    for (const auto& data : pkg.datas) {
        for (const auto& field : data->fields) {
            std::string typeStr = mapType(*field.type);
            if (field.type->kind == TypeKind::Array) {
                std::string baseType = mapType(*field.type->base);
                componentFieldTypes_[data->name + "." + field.name] = baseType;
            } else {
                componentFieldTypes_[data->name + "." + field.name] = typeStr;
            }
        }
    }

    // Extern declarations (skip C standard library functions - compiler/linker will resolve them)
    for (const auto& ext : pkg.externFunctions) {
        if (!Naming::isCReserved(ext->name)) {
            genExternFnDecl(*ext);
        }
    }
    
    // Global world variables (entry point only)
    if (pkg.isEntryPoint) {
        ctx_.writeImplLn("// Global world variables");
        ctx_.writeImplLn("static paani_world_t* __paani_gen_this_w = NULL;");
        for (const auto& use : pkg.uses) {
            std::string pkgName;
            for (size_t i = 0; i < use->packagePath.size(); i++) {
                if (i > 0) pkgName += "__";
                pkgName += std::string(use->packagePath[i]);
            }
            ctx_.writeImplLn("static paani_world_t* __paani_gen_" + pkgName + "_w = NULL;");
        }
        ctx_.writeImplLn("");
    }
    
    // Component type IDs
    for (const auto& data : pkg.datas) {
        ctx_.writeImplLn("paani_ctype_t " + Naming::componentTypeId(packageName_, data->name) + " = (paani_ctype_t)-1;");
    }
    
    // Trait type IDs
    for (const auto& trait : pkg.traits) {
        ctx_.writeImplLn("paani_trait_t " + Naming::traitTypeId(packageName_, trait->name) + " = (paani_trait_t)-1;");
    }
    ctx_.writeImplLn("");
    
    // Function implementations
    for (const auto& fn : pkg.functions) {
        genFnDecl(*fn, false);
    }
    
    // System implementations
    for (const auto& sys : pkg.systems) {
        genSystemDecl(*sys);
    }
    
    // Template implementations
    for (const auto& tmpl : pkg.templates) {
        genTemplateDecl(*tmpl);
    }
    
    // Module init
    ctx_.writeImplLn("// Module initialization");
    ctx_.writeImplLn("void " + Naming::moduleInit(packageName_) + "(paani_world_t* __paani_gen_world) {");
    ctx_.pushIndent();
    
    // Register components
    for (const auto& data : pkg.datas) {
        ctx_.writeImplLn(Naming::componentTypeId(packageName_, data->name) + " = paani_component_register(__paani_gen_world, sizeof(" + Naming::component(packageName_, data->name) + "), NULL, NULL);");
    }
    ctx_.writeImplLn("");
    
    // Register traits
    for (const auto& trait : pkg.traits) {
        if (trait->requiredData.empty()) {
            // Tag trait (no components)
            ctx_.writeImplLn(Naming::traitTypeId(packageName_, trait->name) + " = paani_trait_register(__paani_gen_world, NULL, 0);");
        } else {
            // Component trait
            ctx_.writeImplLn("{");
            ctx_.pushIndent();
            ctx_.writeImplLn("paani_ctype_t __trait_comps[] = {");
            for (size_t i = 0; i < trait->requiredData.size(); i++) {
                if (i > 0) ctx_.writeImpl(", ");
                ctx_.writeImpl(Naming::componentTypeId(packageName_, trait->requiredData[i]));
            }
            ctx_.writeImplLn("};");
            ctx_.writeImplLn(Naming::traitTypeId(packageName_, trait->name) + " = paani_trait_register(__paani_gen_world, __trait_comps, " + 
                             std::to_string(trait->requiredData.size()) + ");");
            ctx_.popIndent();
            ctx_.writeImplLn("}");
        }
    }
    ctx_.writeImplLn("");
    
    // Register systems (initially locked for library packages, unlocked for entry point)
    for (const auto& sys : pkg.systems) {
        std::string sysFuncName = Naming::systemFunction(packageName_, sys->name);
        std::string sysRegName = Naming::system(packageName_, sys->name);
        ctx_.writeImplLn("{");
        ctx_.pushIndent();
        ctx_.writeImplLn("paani_system_t __sys = paani_system_register(__paani_gen_world, \"" + sysRegName + "\", " + 
                         sysFuncName + ", NULL);");
        // Library packages: lock systems by default
        // Entry point: leave systems unlocked (will run automatically)
        if (!pkg.isEntryPoint) {
            ctx_.writeImplLn("paani_system_lock(__paani_gen_world, __sys);  // Lock by default for library packages");
        }
        ctx_.writeImplLn("(void)__sys;");
        ctx_.popIndent();
        ctx_.writeImplLn("}");
    }
    ctx_.writeImplLn("");
    
    // Set up system dependencies
    for (const auto& dep : pkg.dependencies) {
        if (dep->systems.size() >= 2) {
            ctx_.writeImplLn("// Dependency: " + dep->systems[0] + " -> ... -> " + dep->systems[dep->systems.size()-1]);
            ctx_.writeImplLn("{");
            ctx_.pushIndent();
            ctx_.writeImplLn("paani_system_t __before, __after;");
            for (size_t i = 0; i + 1 < dep->systems.size(); i++) {
                std::string before = Naming::system(packageName_, dep->systems[i]);
                std::string after = Naming::system(packageName_, dep->systems[i + 1]);
                ctx_.writeImplLn("__before = paani_system_find(__paani_gen_world, \"" + before + "\");");
                ctx_.writeImplLn("__after = paani_system_find(__paani_gen_world, \"" + after + "\");");
                ctx_.writeImplLn("if (__before != PAANI_INVALID_SYSTEM && __after != PAANI_INVALID_SYSTEM) {");
                ctx_.pushIndent();
                ctx_.writeImplLn("paani_system_depend(__paani_gen_world, __before, __after);");
                ctx_.popIndent();
                ctx_.writeImplLn("}");
            }
            ctx_.popIndent();
            ctx_.writeImplLn("}");
        }
    }
    
    // Global statements for non-entry packages (executed in module_init)
    if (!pkg.isEntryPoint && !pkg.globalStatements.empty()) {
        ctx_.writeImplLn("");
        ctx_.writeImplLn("// Package global initialization code");
        handleTracker_.beginFunction();
        handleEmitter_ = std::make_unique<HandleEmitter>(handleTracker_, ctx_);
        
        // Set current world for global statements
        std::string savedWorld = ctx_.getCurrentWorld();
        ctx_.setCurrentWorld("__paani_gen_world");
        
        for (const auto& stmt : pkg.globalStatements) {
            genStmt(*stmt);
        }
        
        // Emit handle destructors for global handles
        emitAllHandleDtors();
        
        ctx_.setCurrentWorld(savedWorld);
    }
    
    ctx_.popIndent();
    ctx_.writeImplLn("}\n");
    
    // Main function
    if (pkg.isEntryPoint) {
        generateEntryMain(pkg);
    }
    
    return ctx_.impl.str();
}

// Setup methods
inline void CodeGenerator::setupPackage(const Package& pkg) {
    packageName_ = pkg.name;
    isEntryPoint_ = pkg.isEntryPoint;
    typeMapper_.packageName = pkg.name;
    collectFunctionInfo(pkg);
    collectExternInfo(pkg);
    collectHandleInfo(pkg);
    collectTraitInfo(pkg);
    processImports(pkg);
}

inline void CodeGenerator::initializeGenerators() {
    systemGen_ = std::make_unique<SystemGenerator>(ctx_, handleTracker_, packageName_);
    templateGen_ = std::make_unique<TemplateGenerator>(ctx_, packageName_);
    depGen_ = std::make_unique<DependencyGenerator>(ctx_, packageName_);
    
    // Set CodeGenerator pointer for statement generation
    if (systemGen_) systemGen_->setCodeGenerator(this);
    
    // Set trait package map for cross-package trait access
    if (systemGen_) systemGen_->setTraitPackageMap(&traitPackageMap_);
}

inline void CodeGenerator::collectFunctionInfo(const Package& pkg) {
    for (const auto& fn : pkg.functions) {
        if (fn->isExport) {
            exportFunctions_.insert(fn->name);
        } else {
            internalFunctions_.insert(fn->name);
        }
        if (fn->returnType) {
            std::string funcName = fn->isExport ? Naming::exportFunction(packageName_, fn->name) : Naming::internalFunction(packageName_, fn->name);
            functionReturnTypes_[funcName] = fn->returnType->kind;
            if (fn->returnType->kind == TypeKind::UserDefined) {
                functionReturnTypeNames_[funcName] = fn->returnType->name;
            }
        }
    }
}

inline void CodeGenerator::collectExternInfo(const Package& pkg) {
    for (const auto& ext : pkg.externFunctions) {
        externFunctions_.insert(ext->name);
        if (ext->returnType) {
            functionReturnTypes_[ext->name] = ext->returnType->kind;
            if (ext->returnType->kind == TypeKind::UserDefined) {
                functionReturnTypeNames_[ext->name] = ext->returnType->name;
            }
        }
    }
}

inline void CodeGenerator::collectHandleInfo(const Package& pkg) {
    for (const auto& handle : pkg.handles) {
        handleDtors_[handle->name] = handle->dtor;
        typeMapper_.handleTypes.insert(handle->name);
    }
}

inline void CodeGenerator::collectTraitInfo(const Package& pkg) {
    // Store trait -> components mapping for system generation
    for (const auto& trait : pkg.traits) {
        std::vector<std::string> comps;
        for (const auto& comp : trait->requiredData) {
            comps.push_back(std::string(comp));
        }
        traitComponents_[std::string(trait->name)] = std::move(comps);
        traitPackageMap_[std::string(trait->name)] = packageName_;
    }
}

inline void CodeGenerator::processImports(const Package& pkg) {
    for (const auto& use : pkg.uses) {
        std::string pkgName;
        for (size_t i = 0; i < use->packagePath.size(); i++) {
            if (i > 0) pkgName += "__";
            pkgName += std::string(use->packagePath[i]);
        }
        usedPackages_.insert(pkgName);
        
        if (!use->alias.empty()) {
            packageAliases_[std::string(use->alias)] = pkgName;
        }
        
        // Load information from imported package
        // Non-entry point files only load utils/ (functions only)
        bool forceOnlyUtils = !pkg.isEntryPoint;
        bool onlyUtils = forceOnlyUtils || use->onlyUtils;
        PackageInfo* importedPkg = packageManager.loadPackageForUse(use->packagePath, onlyUtils);
        if (importedPkg && importedPkg->ast) {
            // For entry point: load ECS info and functions
            if (pkg.isEntryPoint) {
                // Store component package mapping
                for (const auto& data : importedPkg->ast->datas) {
                    dataPackageMap_[data->name] = pkgName;
                }
                
                // Store trait components
                for (const auto& trait : importedPkg->ast->traits) {
                    std::vector<std::string> comps;
                    for (const auto& comp : trait->requiredData) {
                        comps.push_back(comp);
                    }
                    traitComponents_[trait->name] = std::move(comps);
                    traitPackageMap_[trait->name] = pkgName;
                }
                
                // Store template package mapping
                for (const auto& tmpl : importedPkg->ast->templates) {
                    templatePackageMap_[tmpl->name] = pkgName;
                }
            }
            
            // Load exported functions from imported package (always needed)
            for (const auto& fn : importedPkg->ast->functions) {
                if (fn->isExport && fn->returnType) {
                    // Use same naming convention as semantic analyzer: pkgName__funcName
                    std::string fullFuncName = pkgName + "__" + fn->name;
                    functionReturnTypes_[fullFuncName] = fn->returnType->kind;
                    if (fn->returnType->kind == TypeKind::UserDefined) {
                        functionReturnTypeNames_[fullFuncName] = fn->returnType->name;
                    }
                }
            }
            
            // For entry point: also load extern functions and types
            if (pkg.isEntryPoint) {
                // Load exported extern functions from imported package
                for (const auto& [funcName, funcType] : importedPkg->exportedExternFunctions) {
                    externFunctions_.insert(funcName);
                    if (funcType && funcType->funcReturn) {
                        functionReturnTypes_[funcName] = funcType->funcReturn->kind;
                        if (funcType->funcReturn->kind == TypeKind::UserDefined) {
                            functionReturnTypeNames_[funcName] = funcType->funcReturn->name;
                        }
                    }
                    // Store parameter types
                    if (funcType) {
                        std::vector<TypeKind> paramTypes;
                        for (const auto& param : funcType->funcParams) {
                            if (param) paramTypes.push_back(param->kind);
                        }
                        functionReturnTypes_[funcName] = paramTypes.empty() ? TypeKind::Void : paramTypes[0];
                    }
                }
                
                // Load exported extern types from imported package
                for (const auto& [typeName, typeSize] : importedPkg->exportedExternTypes) {
                    externTypes_.insert(typeName);
                }
            }
        }
    }
}

// Declaration generators
inline void CodeGenerator::genDataDecl(const DataDecl& decl) {
    // Generate component type ID declaration
    ctx_.writeHeaderLn("extern paani_ctype_t " + Naming::componentTypeId(packageName_, decl.name) + ";");
    
    ctx_.writeHeaderLn("typedef struct {");
    for (const auto& field : decl.fields) {
        std::string typeStr = mapType(*field.type);
        // Handle array types specially
        if (field.type->kind == TypeKind::Array) {
            std::string baseType = mapType(*field.type->base);
            ctx_.writeHeaderLn("    " + baseType + " " + field.name + "[" + 
                             std::to_string(field.type->arraySize) + "];");
            // Store field type info (use base type for arrays)
            componentFieldTypes_[decl.name + "." + field.name] = baseType;
        } else {
            ctx_.writeHeaderLn("    " + typeStr + " " + field.name + ";");
            // Store field type info
            componentFieldTypes_[decl.name + "." + field.name] = typeStr;
        }
    }
    ctx_.writeHeaderLn("} " + Naming::component(packageName_, decl.name) + ";\n");
}

inline void CodeGenerator::genTraitDecl(const TraitDecl& decl) {
    ctx_.writeHeaderLn("extern paani_trait_t " + Naming::traitTypeId(packageName_, decl.name) + ";\n");
}

inline void CodeGenerator::genFnDecl(const FnDecl& decl, bool declarationOnly) {
    std::string returnType = mapType(*decl.returnType);
    std::string funcName = decl.isExport ? Naming::exportFunction(packageName_, decl.name) : Naming::internalFunction(packageName_, decl.name);
    
    // Add static for internal functions, extern for exported functions
    std::string storageClass = decl.isExport ? "" : "static ";
    
    std::string sig = storageClass + returnType + " " + funcName + "(paani_world_t* __paani_gen_world";
    for (const auto& param : decl.params) {
        sig += ", " + mapType(*param.type) + " " + param.name;
    }
    sig += ")";
    
    if (declarationOnly) {
        if (decl.isExport) ctx_.writeHeaderLn("extern " + sig + ";");
        return;
    }
    
    ctx_.writeImplLn(sig + " {");
    ctx_.pushIndent();
    
    // Set current world for function body
    std::string savedWorld = ctx_.getCurrentWorld();
    ctx_.setCurrentWorld("__paani_gen_world");
    
    handleTracker_.beginFunction();
    handleEmitter_ = std::make_unique<HandleEmitter>(handleTracker_, ctx_);
    
    for (const auto& param : decl.params) {
        if (isHandleType(*param.type)) {
            auto it = handleDtors_.find(param.type->name);
            if (it != handleDtors_.end()) {
                registerHandleVar(param.name, param.type->name);
            }
        }
    }
    
    if (decl.body) {
        for (const auto& stmt : decl.body->statements) {
            genStmt(*stmt);
        }
    }
    
    emitAllHandleDtors();
    
    // Restore world
    ctx_.setCurrentWorld(savedWorld);
    
    ctx_.popIndent();
    ctx_.writeImplLn("}\n");
}

inline void CodeGenerator::genExternFnDecl(const ExternFnDecl& decl) {
    std::string sig = "extern " + mapType(*decl.returnType) + " " + decl.name + "(";
    for (size_t i = 0; i < decl.params.size(); i++) {
        if (i > 0) sig += ", ";
        sig += mapType(*decl.params[i].type) + " " + decl.params[i].name;
    }
    ctx_.writeImplLn(sig + ");");
}

inline void CodeGenerator::genSystemDecl(const SystemDecl& decl) {
    std::string funcName = Naming::systemFunction(packageName_, decl.name);
    std::string sig = "void " + funcName + 
                      "(paani_world_t* __paani_gen_world, float dt, void* userdata)";
    
    ctx_.writeImplLn(sig + " {");
    ctx_.pushIndent();
    
    // Save and set current world
    std::string savedWorld = ctx_.getCurrentWorld();
    ctx_.setCurrentWorld("__paani_gen_world");
    
    // Setup handle tracking
    handleTracker_.beginFunction();
    handleEmitter_ = std::make_unique<HandleEmitter>(handleTracker_, ctx_);
    
    if (decl.isTraitSystem()) {
        // Get components for the trait
        std::vector<std::string> components;
        if (!decl.forTraits.empty()) {
            std::string traitName = decl.forTraits[0];
            // Strip package prefix if present (e.g., "engine__Movable" -> "Movable")
            size_t sepPos = traitName.find("__");
            if (sepPos != std::string::npos) {
                traitName = traitName.substr(sepPos + 2);
            }
            auto it = traitComponents_.find(traitName);
            if (it != traitComponents_.end()) {
                components = it->second;
            }
        }
        
        // Clear deferred destroys at start
        ctx_.writeImplLn("paani_entity_clear_deferred_destroys(__paani_gen_world);");
        ctx_.writeImplLn("");
        
        if (components.empty()) {
            // No components, generate simple trait query
            generateSimpleTraitSystem(decl);
        } else if (decl.isBatch) {
            // Generate batch-optimized trait system for #batch attribute
            generateBatchTraitSystem(decl, components);
        } else {
            // Generate iterator-based trait system
            generateIteratorTraitSystem(decl, components);
        }
        
        // Process deferred destroys
        ctx_.writeImplLn("");
        ctx_.writeImplLn("paani_entity_process_deferred_destroys(__paani_gen_world);");
    } else {
        // Plain system - generate body directly
        ctx_.writeImplLn("// Plain system: " + decl.name);
        if (decl.body) {
            inTraitSystem_ = false;
            for (const auto& stmt : decl.body->statements) {
                genStmt(*stmt);
            }
        }
    }
    
    // Cleanup handles
    emitAllHandleDtors();
    handleTracker_.endFunction();
    
    // Restore world
    ctx_.setCurrentWorld(savedWorld);
    
    ctx_.popIndent();
    ctx_.writeImplLn("}");
}

// Helper for simple trait system (no components)
inline void CodeGenerator::generateSimpleTraitSystem(const SystemDecl& decl) {
    // Get the correct package for the trait
    std::string traitPkg = packageName_;
    auto it = traitPackageMap_.find(decl.forTraits[0]);
    if (it != traitPackageMap_.end()) {
        traitPkg = it->second;
    }
    
    ctx_.writeImplLn("// Trait system (no components): " + decl.name);
    ctx_.writeImplLn("paani_query_t* __trait_query = paani_query_trait(__paani_gen_world, " + 
                     Naming::traitTypeId(traitPkg, decl.forTraits[0]) + ");");
    ctx_.writeImplLn("paani_query_result_t __trait_result;");
    ctx_.writeImplLn("__trait_result.components = NULL;");
    ctx_.writeImplLn("__trait_result.capacity = 0;");
    ctx_.writeImplLn("");
    ctx_.writeImplLn("while (paani_query_next(__trait_query, &__trait_result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn("paani_entity_t " + decl.entityVarName + " = __trait_result.entity;");
    ctx_.writeImplLn("(void)" + decl.entityVarName + ";");
    ctx_.writeImplLn("");
    
    // Generate body
    inTraitSystem_ = true;
    inBatchSystem_ = false;  // Simple trait system uses pointer access
    componentPointers_.clear();
    if (decl.body) {
        for (const auto& stmt : decl.body->statements) {
            genStmt(*stmt);
        }
    }
    inTraitSystem_ = false;
    inBatchSystem_ = false;
    componentPointers_.clear();

    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.writeImplLn("paani_query_destroy(__trait_query);");
}

// Helper for iterator-based trait system
inline void CodeGenerator::generateIteratorTraitSystem(const SystemDecl& decl,
                                                        const std::vector<std::string>& components) {
    size_t compCount = components.size();
    
    // Parse trait name - may contain package prefix (e.g., "engine__Movable")
    std::string traitName = decl.forTraits[0];
    std::string traitPkg = packageName_;
    
    size_t sepPos = traitName.find("__");
    if (sepPos != std::string::npos) {
        // Explicit package prefix: package__TraitName
        traitPkg = traitName.substr(0, sepPos);
        traitName = traitName.substr(sepPos + 2);
    } else {
        // No prefix, look up in traitPackageMap_
        auto traitIt = traitPackageMap_.find(traitName);
        if (traitIt != traitPackageMap_.end()) {
            traitPkg = traitIt->second;
        }
    }
    
    // Determine the correct world variable for the trait
    // Use current world from context (for entry point, it's __paani_gen_this_w)
    std::string currentWorld = ctx_.getCurrentWorld();
    std::string traitWorld = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
    if (traitPkg != packageName_) {
        // Cross-package access: use the target package's world variable
        traitWorld = "__paani_gen_" + traitPkg + "_w";
    }
    
    ctx_.writeImplLn("// Trait system: " + decl.name);
    ctx_.writeImplLn("paani_query_t* __trait_query = paani_query_trait(" + traitWorld + ", " + 
                     Naming::traitTypeId(traitPkg, traitName) + ");");    ctx_.writeImplLn("paani_query_result_t __trait_result;");
    ctx_.writeImplLn("void* __trait_comp_buffer[" + std::to_string(compCount) + "];");
    ctx_.writeImplLn("__trait_result.components = __trait_comp_buffer;");
    ctx_.writeImplLn("__trait_result.capacity = " + std::to_string(compCount) + ";");
    ctx_.writeImplLn("");
    ctx_.writeImplLn("while (paani_query_next(__trait_query, &__trait_result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn("paani_entity_t " + decl.entityVarName + " = __trait_result.entity;");
    ctx_.writeImplLn("(void)" + decl.entityVarName + ";");
    ctx_.writeImplLn("");
    
    // Generate component pointers - use component's actual package
    componentPointers_.clear();
    for (size_t i = 0; i < components.size(); i++) {
        std::string compName = components[i];
        // Get component's package (may be different from current package)
        std::string compPkg = packageName_;
        auto compIt = traitPackageMap_.find(compName);
        if (compIt != traitPackageMap_.end()) {
            compPkg = compIt->second;
        }
        std::string compTypeName = Naming::component(compPkg, compName);
        std::string pointerName = "_" + decl.entityVarName + "_" + compName;
        
        ctx_.writeImplLn(compTypeName + "* " + pointerName + " = (" + compTypeName + 
                         "*)__trait_result.components[" + std::to_string(i) + "];");
        componentPointers_[decl.entityVarName + "." + compName] = pointerName;
    }
    ctx_.writeImplLn("");
    
    // Generate body
    inTraitSystem_ = true;
    inBatchSystem_ = false;  // Iterator system uses pointer access
    if (decl.body) {
        for (const auto& stmt : decl.body->statements) {
            genStmt(*stmt);
        }
    }
    inTraitSystem_ = false;
    inBatchSystem_ = false;
    componentPointers_.clear();

    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.writeImplLn("paani_query_destroy(__trait_query);");
}

// Helper for batch-optimized trait system (#batch attribute)
// Generates direct archetype access with zero function calls in hot loop
// Achieves maximum performance through direct column pointer access
inline void CodeGenerator::generateBatchTraitSystem(const SystemDecl& decl,
                                                     const std::vector<std::string>& components) {
    size_t compCount = components.size();

    // Parse trait name - may contain package prefix (e.g., "engine__Movable")
    std::string traitName = decl.forTraits[0];
    std::string traitPkg = packageName_;
    
    size_t sepPos = traitName.find("__");
    if (sepPos != std::string::npos) {
        // Explicit package prefix: package__TraitName
        traitPkg = traitName.substr(0, sepPos);
        traitName = traitName.substr(sepPos + 2);
    } else {
        // No prefix, look up in traitPackageMap_
        auto traitIt = traitPackageMap_.find(traitName);
        if (traitIt != traitPackageMap_.end()) {
            traitPkg = traitIt->second;
        }
    }

    // Determine the correct world variable for the trait
    // Use current world from context (for entry point, it's __paani_gen_this_w)
    std::string currentWorld = ctx_.getCurrentWorld();
    std::string traitWorld = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
    if (traitPkg != packageName_) {
        // Cross-package access: use the target package's world variable
        traitWorld = "__paani_gen_" + traitPkg + "_w";
    }

    ctx_.writeImplLn("// Batch-optimized trait system: " + decl.name);
    ctx_.writeImplLn("// Direct batch access - minimal function calls");
    ctx_.writeImplLn("paani_query_t* __trait_query = paani_query_trait(" + traitWorld + ", " +
                     Naming::traitTypeId(traitPkg, traitName) + ");");
    ctx_.writeImplLn("uint32_t __count = paani_query_batch_count(__trait_query);");
    ctx_.writeImplLn("");

    // Generate direct component column pointers using batch API
    ctx_.writeImplLn("// Direct component column pointers");
    componentPointers_.clear();
    for (size_t i = 0; i < components.size(); i++) {
        std::string compName = components[i];
        std::string compPkg = packageName_;
        auto compIt = traitPackageMap_.find(compName);
        if (compIt != traitPackageMap_.end()) {
            compPkg = compIt->second;
        }
        std::string compTypeName = Naming::component(compPkg, compName);
        std::string colName = "__col_" + compName;

        // Use batch API to get column pointer
        ctx_.writeImplLn(compTypeName + "* __restrict " + colName + " = (" + compTypeName +
                         "*)paani_query_batch_column(__trait_query, " + std::to_string(i) + ");");
        componentPointers_[decl.entityVarName + "." + compName] = colName;
    }
    ctx_.writeImplLn("");

    // Generate batch loop with compiler hints
    ctx_.writeImplLn("// Batch process all entities - compiler can auto-vectorize");
    ctx_.writeImplLn("#ifdef __clang__");
    ctx_.writeImplLn("#pragma clang loop vectorize(enable) interleave(enable)");
    ctx_.writeImplLn("#elif defined(__GNUC__)");
    ctx_.writeImplLn("#pragma GCC ivdep");
    ctx_.writeImplLn("#endif");
    ctx_.writeImplLn("for (uint32_t __idx = 0; __idx < __count; __idx++) {");
    ctx_.pushIndent();

    // Generate body
    inTraitSystem_ = true;
    inBatchSystem_ = true;
    if (decl.body) {
        for (const auto& stmt : decl.body->statements) {
            genStmt(*stmt);
        }
    }
    inTraitSystem_ = false;
    inBatchSystem_ = false;
    componentPointers_.clear();

    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.writeImplLn("paani_query_destroy(__trait_query);");
}

inline void CodeGenerator::genTemplateDecl(const TemplateDecl& decl) {
    if (templateGen_) {
        templateGen_->generate(decl);
    }
}

inline void CodeGenerator::genDependencyDecl(const DependencyDecl& decl) {
    if (depGen_) {
        depGen_->generate(decl);
    }
}

// Statement generators
inline void CodeGenerator::genStmt(const Stmt& stmt) {
    if (auto* s = dynamic_cast<const BlockStmt*>(&stmt)) genBlock(*s);
    else if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt)) genVarDecl(*s);
    else if (auto* s = dynamic_cast<const ExprStmt*>(&stmt)) genExprStmt(*s);
    else if (auto* s = dynamic_cast<const IfStmt*>(&stmt)) genIf(*s);
    else if (auto* s = dynamic_cast<const WhileStmt*>(&stmt)) genWhile(*s);
    else if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt)) genReturn(*s);
    else if (auto* s = dynamic_cast<const BreakStmt*>(&stmt)) genBreak(*s);
    else if (auto* s = dynamic_cast<const ContinueStmt*>(&stmt)) genContinue(*s);
    else if (auto* s = dynamic_cast<const GiveStmt*>(&stmt)) genGive(*s);
    else if (auto* s = dynamic_cast<const TagStmt*>(&stmt)) genTag(*s);
    else if (auto* s = dynamic_cast<const UntagStmt*>(&stmt)) genUntag(*s);
    else if (auto* s = dynamic_cast<const DestroyStmt*>(&stmt)) genDestroy(*s);
    else if (auto* s = dynamic_cast<const QueryStmt*>(&stmt)) genQuery(*s);
    else if (auto* s = dynamic_cast<const LockStmt*>(&stmt)) genLock(*s);
    else if (auto* s = dynamic_cast<const UnlockStmt*>(&stmt)) genUnlock(*s);
    else if (auto* s = dynamic_cast<const ExitStmt*>(&stmt)) genExit(*s);
}

inline void CodeGenerator::genQuery(const QueryStmt& stmt) {
    std::string entityVar = stmt.varName.empty() ? "e" : stmt.varName;
    
    if (stmt.traitNames.size() > 1) {
        // Multi-trait query
        generateMultiTraitQueryImpl(stmt, entityVar);
    } else if (!stmt.traitNames.empty()) {
        // Single trait query
        generateSingleTraitQueryImpl(stmt, entityVar);
    } else {
        // Component-only query
        generateComponentQueryImpl(stmt, entityVar);
    }
}

// Helper: Single trait query
inline void CodeGenerator::generateSingleTraitQueryImpl(const QueryStmt& stmt, const std::string& entityVar) {
    std::string traitName = stmt.traitNames[0];
    
    // Get components for this trait
    std::vector<std::string> components;
    auto it = traitComponents_.find(traitName);
    if (it != traitComponents_.end()) {
        components = it->second;
    }
    
    // Add withComponents if specified
    for (const auto& comp : stmt.withComponents) {
        bool alreadyIncluded = false;
        const auto& c = comp;
        for (const auto& existing : components) {
            if (existing == c) {
                alreadyIncluded = true;
                break;
            }
        }
        if (!alreadyIncluded) {
            components.push_back(std::string(comp));
        }
    }
    
    size_t compCount = components.size();
    size_t bufferSize = compCount > 0 ? compCount : 1;
    
    ctx_.writeImplLn(ctx_.indent() + "// Query: trait " + traitName);
    ctx_.writeImplLn(ctx_.indent() + "{");
    ctx_.pushIndent();
    // Use current world from context (for entry point, it's __paani_gen_this_w)
    std::string currentWorld = ctx_.getCurrentWorld();
    std::string queryWorld = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
    ctx_.writeImplLn(ctx_.indent() + "paani_query_t* __query = paani_query_trait(" + queryWorld + ", " + 
                     Naming::traitTypeId(packageName_, traitName) + ");");
    ctx_.writeImplLn(ctx_.indent() + "paani_query_result_t __result;");
    ctx_.writeImplLn(ctx_.indent() + "void* __comp_buffer[" + std::to_string(bufferSize) + "];");
    ctx_.writeImplLn(ctx_.indent() + "__result.components = __comp_buffer;");
    ctx_.writeImplLn(ctx_.indent() + "__result.capacity = " + std::to_string(bufferSize) + ";");
    ctx_.writeImplLn(ctx_.indent() + "");
    ctx_.writeImplLn(ctx_.indent() + "while (paani_query_next(__query, &__result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn(ctx_.indent() + "paani_entity_t " + entityVar + " = __result.entity;");
    ctx_.writeImplLn(ctx_.indent() + "(void)" + entityVar + ";");
    
    // Generate component pointers
    if (!components.empty()) {
        ctx_.writeImplLn(ctx_.indent() + "");
        for (size_t i = 0; i < components.size(); i++) {
            std::string compName = components[i];
            std::string compTypeName = Naming::component(packageName_, compName);
            std::string pointerName = "_" + entityVar + "_" + compName;
            ctx_.writeImplLn(ctx_.indent() + compTypeName + "* " + pointerName + " = (" + compTypeName + 
                             "*)__result.components[" + std::to_string(i) + "];");
            componentPointers_[entityVar + "." + compName] = pointerName;
        }
    }
    ctx_.writeImplLn(ctx_.indent() + "");
    
    // Generate body
    inTraitSystem_ = true;
    inBatchSystem_ = false;  // Query uses iterator pattern with pointer access
    if (stmt.body) {
        for (const auto& bodyStmt : stmt.body->statements) {
            genStmt(*bodyStmt);
        }
    }
    inTraitSystem_ = false;
    inBatchSystem_ = false;

    // Clear component pointers
    for (const auto& comp : components) {
        componentPointers_.erase(entityVar + "." + comp);
    }
    
    ctx_.popIndent();
    ctx_.writeImplLn(ctx_.indent() + "}");
    ctx_.writeImplLn(ctx_.indent() + "paani_query_destroy(__query);");
    ctx_.popIndent();
    ctx_.writeImplLn(ctx_.indent() + "}");
}

// Helper: Component-only query
inline void CodeGenerator::generateComponentQueryImpl(const QueryStmt& stmt, const std::string& entityVar) {
    size_t compCount = stmt.withComponents.size();
    size_t bufferSize = compCount > 0 ? compCount : 1;
    
    ctx_.writeImplLn(ctx_.indent() + "// Query: by components");
    ctx_.writeImplLn(ctx_.indent() + "{");
    ctx_.pushIndent();
    
    // Use current world from context (for entry point, it's __paani_gen_this_w)
    std::string currentWorld = ctx_.getCurrentWorld();
    std::string queryWorld = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
    
    if (compCount == 0) {
        ctx_.writeImplLn(ctx_.indent() + "paani_query_t* __query = paani_query_create(" + queryWorld + ", NULL, 0);");
    } else {
        ctx_.writeImplLn(ctx_.indent() + "paani_ctype_t __comp_types[] = {");
        for (size_t i = 0; i < compCount; i++) {
            if (i > 0) ctx_.writeImpl(", ");
            ctx_.writeImpl(Naming::componentTypeId(packageName_, stmt.withComponents[i]));
        }
        ctx_.writeImplLn("};");
        ctx_.writeImplLn(ctx_.indent() + "paani_query_t* __query = paani_query_create(" + queryWorld + ", __comp_types, " + 
                         std::to_string(compCount) + ");");
    }
    
    ctx_.writeImplLn(ctx_.indent() + "paani_query_result_t __result;");
    ctx_.writeImplLn(ctx_.indent() + "void* __comp_buffer[" + std::to_string(bufferSize) + "];");
    ctx_.writeImplLn(ctx_.indent() + "__result.components = __comp_buffer;");
    ctx_.writeImplLn(ctx_.indent() + "__result.capacity = " + std::to_string(bufferSize) + ";");
    ctx_.writeImplLn(ctx_.indent() + "");
    ctx_.writeImplLn(ctx_.indent() + "while (paani_query_next(__query, &__result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn(ctx_.indent() + "paani_entity_t " + entityVar + " = __result.entity;");
    ctx_.writeImplLn(ctx_.indent() + "(void)" + entityVar + ";");
    
    // Generate component pointers
    if (!stmt.withComponents.empty()) {
        ctx_.writeImplLn(ctx_.indent() + "");
        for (size_t i = 0; i < stmt.withComponents.size(); i++) {
            std::string compName = stmt.withComponents[i];
            std::string compTypeName = Naming::component(packageName_, compName);
            std::string pointerName = "_" + entityVar + "_" + compName;
            ctx_.writeImplLn(ctx_.indent() + compTypeName + "* " + pointerName + " = (" + compTypeName + 
                             "*)__result.components[" + std::to_string(i) + "];");
            componentPointers_[entityVar + "." + compName] = pointerName;
        }
        ctx_.writeImplLn(ctx_.indent() + "");
    }
    
    // Generate body
    inTraitSystem_ = true;
    inBatchSystem_ = false;  // For loop uses iterator pattern with pointer access
    if (stmt.body) {
        for (const auto& bodyStmt : stmt.body->statements) {
            genStmt(*bodyStmt);
        }
    }
    inTraitSystem_ = false;
    inBatchSystem_ = false;

    // Clear component pointers
    for (const auto& comp : stmt.withComponents) {
        componentPointers_.erase(entityVar + "." + std::string(comp));
    }
    
    ctx_.popIndent();
    ctx_.writeImplLn(ctx_.indent() + "}");
    ctx_.writeImplLn(ctx_.indent() + "paani_query_destroy(__query);");
    ctx_.popIndent();
    ctx_.writeImplLn(ctx_.indent() + "}");
}

// Helper: Multi-trait query
inline void CodeGenerator::generateMultiTraitQueryImpl(const QueryStmt& stmt, const std::string& entityVar) {
    // Get max component count
    size_t maxCompCount = 0;
    for (const auto& traitName : stmt.traitNames) {
        auto it = traitComponents_.find(std::string(traitName));
        if (it != traitComponents_.end()) {
            maxCompCount = std::max(maxCompCount, it->second.size());
        }
    }
    
    // Add withComponents
    for (const auto& comp : stmt.withComponents) {
        maxCompCount++;
    }
    
    size_t bufferSize = maxCompCount > 0 ? maxCompCount : 1;
    
    ctx_.writeImplLn(ctx_.indent() + "// Query: multi-trait");
    ctx_.writeImplLn(ctx_.indent() + "{");
    ctx_.pushIndent();
    // Use current world from context (for entry point, it's __paani_gen_this_w)
    std::string currentWorld = ctx_.getCurrentWorld();
    std::string queryWorld = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
    ctx_.writeImplLn(ctx_.indent() + "paani_query_t* __query = paani_query_trait(" + queryWorld + ", " + 
                     Naming::traitTypeId(packageName_, stmt.traitNames[0]) + ");");
    ctx_.writeImplLn(ctx_.indent() + "paani_query_result_t __result;");
    ctx_.writeImplLn(ctx_.indent() + "void* __comp_buffer[" + std::to_string(bufferSize) + "];");
    ctx_.writeImplLn(ctx_.indent() + "__result.components = __comp_buffer;");
    ctx_.writeImplLn(ctx_.indent() + "__result.capacity = " + std::to_string(bufferSize) + ";");
    ctx_.writeImplLn(ctx_.indent() + "");
    ctx_.writeImplLn(ctx_.indent() + "while (paani_query_next(__query, &__result)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn(ctx_.indent() + "paani_entity_t " + entityVar + " = __result.entity;");
    ctx_.writeImplLn(ctx_.indent() + "(void)" + entityVar + ";");
    ctx_.writeImplLn(ctx_.indent() + "");
    
    // Filter by additional traits
    for (size_t i = 1; i < stmt.traitNames.size(); i++) {
        ctx_.writeImplLn(ctx_.indent() + "if (!paani_entity_has_trait(" + queryWorld + ", " + entityVar + 
                         ", " + Naming::traitTypeId(packageName_, stmt.traitNames[i]) + ")) continue;");
    }
    
    // Generate component pointers from first trait
    auto it = traitComponents_.find(std::string(stmt.traitNames[0]));
    if (it != traitComponents_.end()) {
        ctx_.writeImplLn(ctx_.indent() + "");
        for (size_t i = 0; i < it->second.size(); i++) {
            std::string compName = it->second[i];
            std::string compTypeName = Naming::component(packageName_, compName);
            std::string pointerName = "_" + entityVar + "_" + compName;
            ctx_.writeImplLn(ctx_.indent() + compTypeName + "* " + pointerName + " = (" + compTypeName + 
                             "*)__result.components[" + std::to_string(i) + "];");
            componentPointers_[entityVar + "." + compName] = pointerName;
        }
    }
    
    // Add withComponents pointers
    for (size_t i = 0; i < stmt.withComponents.size(); i++) {
        std::string compName = stmt.withComponents[i];
        std::string compTypeName = Naming::component(packageName_, compName);
        std::string pointerName = "_" + entityVar + "_" + compName;
        ctx_.writeImplLn(ctx_.indent() + compTypeName + "* " + pointerName + " = paani_component_get(__paani_gen_world, " + 
                         entityVar + ", " + Naming::componentTypeId(packageName_, compName) + ");");
        componentPointers_[entityVar + "." + compName] = pointerName;
    }
    ctx_.writeImplLn(ctx_.indent() + "");
    
    // Generate body
    inTraitSystem_ = true;
    inBatchSystem_ = false;  // Lock statement uses pointer access
    if (stmt.body) {
        for (const auto& bodyStmt : stmt.body->statements) {
            genStmt(*bodyStmt);
        }
    }
    inTraitSystem_ = false;
    inBatchSystem_ = false;

    // Clear component pointers
    if (it != traitComponents_.end()) {
        for (const auto& comp : it->second) {
            componentPointers_.erase(entityVar + "." + comp);
        }
    }
    for (const auto& comp : stmt.withComponents) {
        componentPointers_.erase(entityVar + "." + std::string(comp));
    }
    
    ctx_.popIndent();
    ctx_.writeImplLn(ctx_.indent() + "}");
    ctx_.writeImplLn(ctx_.indent() + "paani_query_destroy(__query);");
    ctx_.popIndent();
    ctx_.writeImplLn(ctx_.indent() + "}");
}

inline void CodeGenerator::genBlock(const BlockStmt& stmt) {
    ctx_.writeImplLn("{");
    ctx_.pushIndent();
    handleTracker_.pushScope();
    
    for (const auto& s : stmt.statements) {
        genStmt(*s);
    }
    
    emitHandleDtorsForCurrentScope();
    handleTracker_.popScope();
    ctx_.popIndent();
    ctx_.writeImplLn("}");
}

inline void CodeGenerator::genVarDecl(const VarDeclStmt& stmt) {
    std::string type;
    std::string handleTypeName;
    
    if (stmt.type) {
        type = mapType(*stmt.type);
        if (stmt.type->kind == TypeKind::UserDefined && typeMapper_.handleTypes.count(stmt.type->name)) {
            handleTypeName = stmt.type->name;
        }
    } else if (stmt.init) {
        type = inferType(*stmt.init);
        if (auto* call = dynamic_cast<const CallExpr*>(stmt.init.get())) {
            if (auto* id = dynamic_cast<const IdentifierExpr*>(call->callee.get())) {
                auto it = functionReturnTypeNames_.find(id->name);
                if (it != functionReturnTypeNames_.end() && typeMapper_.handleTypes.count(it->second)) {
                    handleTypeName = it->second;
                }
            }
        }
        if (auto* id = dynamic_cast<const IdentifierExpr*>(stmt.init.get())) {
            if (handleTracker_.isHandle(id->name)) {
                auto it = handleTracker_.varTypes.find(id->name);
                if (it != handleTracker_.varTypes.end()) handleTypeName = it->second;
            }
        }
    } else {
        type = "int32_t";
    }
    
    varTypes_[stmt.name] = type;
    if (!handleTypeName.empty()) registerHandleVar(stmt.name, handleTypeName);
    
    // Track entity world for spawn expressions and function calls
    if (type == "paani_entity_t" && stmt.init) {
        if (auto* spawnExpr = dynamic_cast<const SpawnExpr*>(stmt.init.get())) {
            std::string entityWorld;
            if (!spawnExpr->templateName.empty()) {
                auto it = templatePackageMap_.find(spawnExpr->templateName);
                if (it != templatePackageMap_.end() && it->second != packageName_) {
                    // Cross-package template: use template's package world
                    entityWorld = "__paani_gen_" + it->second + "_w";
                } else {
                    // Local template: use current context's world
                    entityWorld = ctx_.getCurrentWorld();
                }
            } else {
                // Empty spawn: use current context's world
                entityWorld = ctx_.getCurrentWorld();
            }
            // Fallback to default if context world is empty
            if (entityWorld.empty()) {
                entityWorld = "__paani_gen_world";
            }
            entityWorlds_[stmt.name] = entityWorld;
        } else if (auto* callExpr = dynamic_cast<const CallExpr*>(stmt.init.get())) {
            // Track entity world for function calls that return entity
            // Check if it's a cross-package function call (e.g., engine.create_player())
            if (auto* memberExpr = dynamic_cast<const MemberExpr*>(callExpr->callee.get())) {
                if (auto* pkgId = dynamic_cast<const IdentifierExpr*>(memberExpr->object.get())) {
                    std::string pkgName = pkgId->name;
                    // Resolve alias to actual package name
                    auto aliasIt = packageAliases_.find(pkgName);
                    if (aliasIt != packageAliases_.end()) {
                        pkgName = aliasIt->second;
                    }
                    // If calling another package's function, entity belongs to that package's world
                    if (usedPackages_.count(pkgName) && pkgName != packageName_) {
                        entityWorlds_[stmt.name] = "__paani_gen_" + pkgName + "_w";
                    } else {
                        entityWorlds_[stmt.name] = "__paani_gen_world";
                    }
                }
            } else {
                // Local function call - entity belongs to current world
                entityWorlds_[stmt.name] = "__paani_gen_world";
            }
        }
    }
    
    // Special handling for f-string: generate f-string code first, then declare variable
    if (stmt.init && dynamic_cast<const FStringExpr*>(stmt.init.get())) {
        std::string fstrResult = genExpr(*stmt.init);
        ctx_.writeImplLn(ctx_.indent() + type + " " + stmt.name + " = " + fstrResult + ";");
        return;
    }
    
    // Special handling for array literal
    if (stmt.init && dynamic_cast<const ArrayLiteralExpr*>(stmt.init.get())) {
        auto* arrLit = dynamic_cast<const ArrayLiteralExpr*>(stmt.init.get());
        std::string elemType = "int32_t";
        if (!arrLit->elements.empty()) {
            elemType = inferType(*arrLit->elements[0]);
        }
        size_t arrSize = arrLit->elements.size();
        
        // Generate: type name[] = {elem1, elem2, ...};
        ctx_.writeImplLn(ctx_.indent() + elemType + " " + stmt.name + "[" + std::to_string(arrSize) + "] = {");
        ctx_.pushIndent();
        for (size_t i = 0; i < arrLit->elements.size(); i++) {
            if (i > 0) ctx_.writeImpl(", ");
            ctx_.writeImpl(genExpr(*arrLit->elements[i]));
        }
        ctx_.writeImplLn("");
        ctx_.popIndent();
        ctx_.writeImplLn(ctx_.indent() + "};");
        return;
    }
    
    ctx_.writeImpl(ctx_.indent() + type + " " + stmt.name);
    
    if (stmt.init) {
        if (auto* id = dynamic_cast<const IdentifierExpr*>(stmt.init.get())) {
            if (handleTracker_.isHandle(id->name)) {
                ctx_.writeImplLn(" = " + std::string(id->name) + ";");
                emitHandleMove(id->name);
                return;
            }
        }
        ctx_.writeImpl(" = " + genExpr(*stmt.init));
    }
    ctx_.writeImplLn(";");
}

inline void CodeGenerator::genExprStmt(const ExprStmt& stmt) {
    if (auto* bin = dynamic_cast<const BinaryExpr*>(stmt.expr.get())) {
        if (bin->op == BinOp::Assign) {
            if (auto* rightId = dynamic_cast<const IdentifierExpr*>(bin->right.get())) {
                if (handleTracker_.isHandle(rightId->name)) {
                    std::string left = genExpr(*bin->left);
                    ctx_.writeImplLn(ctx_.indent() + left + " = " + rightId->name + ";");
                    emitHandleMove(rightId->name);
                    return;
                }
            }
        }
    }
    
    if (auto* call = dynamic_cast<const CallExpr*>(stmt.expr.get())) {
        std::vector<std::string> handleArgs;
        for (const auto& arg : call->args) {
            if (auto* id = dynamic_cast<const IdentifierExpr*>(arg.get())) {
                if (handleTracker_.isHandle(id->name)) handleArgs.push_back(id->name);
            }
        }
        if (!handleArgs.empty()) {
            ctx_.writeImplLn(ctx_.indent() + genExpr(*stmt.expr) + ";");
            for (const auto& arg : handleArgs) emitHandleMove(arg);
            return;
        }
    }
    
    ctx_.writeImplLn(ctx_.indent() + genExpr(*stmt.expr) + ";");
}

inline void CodeGenerator::genIf(const IfStmt& stmt) {
    ctx_.writeImplLn(ctx_.indent() + "if (" + genExpr(*stmt.condition) + ")");
    genStmt(*stmt.thenBranch);
    if (stmt.elseBranch) {
        ctx_.writeImplLn(ctx_.indent() + "else");
        genStmt(*stmt.elseBranch);
    }
}

inline void CodeGenerator::genWhile(const WhileStmt& stmt) {
    ctx_.writeImplLn(ctx_.indent() + "while (" + genExpr(*stmt.condition) + ")");
    genStmt(*stmt.body);
}

inline void CodeGenerator::genReturn(const ReturnStmt& stmt) {
    if (stmt.value) {
        if (auto* id = dynamic_cast<const IdentifierExpr*>(stmt.value.get())) {
            if (handleTracker_.isHandle(id->name)) handleTracker_.markMoved(id->name);
        }
    }
    emitAllHandleDtors();
    ctx_.writeImpl(ctx_.indent() + "return");
    if (stmt.value) ctx_.writeImpl(" " + genExpr(*stmt.value));
    ctx_.writeImplLn(";");
}

inline void CodeGenerator::genBreak(const BreakStmt&) {
    ctx_.writeImplLn(ctx_.indent() + "break;");
}

inline void CodeGenerator::genContinue(const ContinueStmt&) {
    ctx_.writeImplLn(ctx_.indent() + "continue;");
}

inline void CodeGenerator::genGive(const GiveStmt& stmt) {
    std::string entity = genExpr(*stmt.entity);
    
    // Parse component name (may have package prefix like "engine.Health")
    std::string compName = stmt.componentName;
    std::string compPkg = packageName_;
    size_t dotPos = stmt.componentName.find('.');
    if (dotPos != std::string::npos) {
        // Has package prefix: "package.ComponentName"
        compPkg = stmt.componentName.substr(0, dotPos);
        compName = stmt.componentName.substr(dotPos + 1);
    } else {
        // No package prefix, look up in dataPackageMap_
        auto it = dataPackageMap_.find(stmt.componentName);
        if (it != dataPackageMap_.end()) {
            compPkg = it->second;
        }
    }
    
    std::string compTypeName = Naming::component(compPkg, compName);
    std::string compTypeId = Naming::componentTypeId(compPkg, compName);
    
    // Determine world: component belongs to its defining package's world
    // Use current world from context (for entry point, it's __paani_gen_this_w)
    std::string currentWorld = ctx_.getCurrentWorld();
    std::string worldParam = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
    if (compPkg != packageName_) {
        worldParam = "__paani_gen_" + compPkg + "_w";
    }
    
    if (stmt.initFields.empty()) {
        // Simple give without initialization
        ctx_.writeImplLn(ctx_.indent() + "paani_component_add(" + worldParam + ", " + entity + ", " + compTypeId + ", NULL);");
    } else {
        // Give with field initialization
        ctx_.writeImplLn(ctx_.indent() + "{");
        ctx_.pushIndent();
        ctx_.writeImplLn(ctx_.indent() + compTypeName + " __comp_data = {0};");
        
        // Initialize each field
        for (const auto& field : stmt.initFields) {
            std::string value = genExpr(*field.second);
            ctx_.writeImplLn(ctx_.indent() + "__comp_data." + field.first + " = " + value + ";");
        }
        
        ctx_.writeImplLn(ctx_.indent() + "paani_component_add(" + worldParam + ", " + entity + ", " + compTypeId + ", &__comp_data);");
        ctx_.popIndent();
        ctx_.writeImplLn(ctx_.indent() + "}");
    }
}

inline void CodeGenerator::genTag(const TagStmt& stmt) {
    std::string entity = genExpr(*stmt.entity);
    
    // Parse trait name (may have package prefix like "engine.TraitName")
    std::string traitName = stmt.traitName;
    std::string traitPkg = packageName_;
    size_t dotPos = stmt.traitName.find('.');
    if (dotPos != std::string::npos) {
        // Has package prefix: "package.TraitName"
        traitPkg = stmt.traitName.substr(0, dotPos);
        traitName = stmt.traitName.substr(dotPos + 1);
    } else {
        // No package prefix, look up in traitPackageMap_
        auto it = traitPackageMap_.find(stmt.traitName);
        if (it != traitPackageMap_.end()) {
            traitPkg = it->second;
        }
    }
    
    std::string traitTypeId = Naming::traitTypeId(traitPkg, traitName);
    
    // Determine world: trait belongs to its defining package's world
    // Use current world from context (for entry point, it's __paani_gen_this_w)
    std::string currentWorld = ctx_.getCurrentWorld();
    std::string worldParam = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
    if (traitPkg != packageName_) {
        worldParam = "__paani_gen_" + traitPkg + "_w";
    }
    
    ctx_.writeImplLn(ctx_.indent() + "paani_trait_add(" + worldParam + ", " + entity + ", " + traitTypeId + ");");
}

inline void CodeGenerator::genUntag(const UntagStmt& stmt) {
    std::string entity = genExpr(*stmt.entity);
    
    // Parse trait name (may have package prefix like "engine.TraitName")
    std::string traitName = stmt.traitName;
    std::string traitPkg = packageName_;
    size_t dotPos = stmt.traitName.find('.');
    if (dotPos != std::string::npos) {
        // Has package prefix: "package.TraitName"
        traitPkg = stmt.traitName.substr(0, dotPos);
        traitName = stmt.traitName.substr(dotPos + 1);
    } else {
        // No package prefix, look up in traitPackageMap_
        auto it = traitPackageMap_.find(stmt.traitName);
        if (it != traitPackageMap_.end()) {
            traitPkg = it->second;
        }
    }
    
    std::string traitTypeId = Naming::traitTypeId(traitPkg, traitName);
    
    // Determine world: trait belongs to its defining package's world
    // Use current world from context (for entry point, it's __paani_gen_this_w)
    std::string currentWorld = ctx_.getCurrentWorld();
    std::string worldParam = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
    if (traitPkg != packageName_) {
        worldParam = "__paani_gen_" + traitPkg + "_w";
    }
    
    ctx_.writeImplLn(ctx_.indent() + "paani_trait_remove(" + worldParam + ", " + entity + ", " + traitTypeId + ");");
}

inline void CodeGenerator::genDestroy(const DestroyStmt& stmt) {
    if (stmt.entity) {
        // Explicit destroy: infer world from the entity's world
        std::string worldParam = getEntityWorld(*stmt.entity);
        ctx_.writeImplLn(ctx_.indent() + "paani_entity_destroy(" + worldParam + ", " + genExpr(*stmt.entity) + ");");
    } else {
        // Implicit destroy in trait system: use current context's world
        std::string currentWorld = ctx_.getCurrentWorld();
        std::string worldParam = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
        ctx_.writeImplLn(ctx_.indent() + "paani_entity_destroy_deferred(" + worldParam + ", e);");
    }
}

inline void CodeGenerator::genExit(const ExitStmt&) {
    emitAllHandleDtors();
    std::string worldParam = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
    ctx_.writeImplLn(ctx_.indent() + "paani_exit(" + worldParam + ");");
}

// Expression generators
inline std::string CodeGenerator::genExpr(const Expr& expr) {
    if (auto* e = dynamic_cast<const IntegerExpr*>(&expr)) return std::to_string(e->value);
    if (auto* e = dynamic_cast<const FloatExpr*>(&expr)) return std::to_string(e->value) + "f";
    if (auto* e = dynamic_cast<const BoolExpr*>(&expr)) return e->value ? "1" : "0";
    if (auto* e = dynamic_cast<const StringExpr*>(&expr)) return "\"" + std::string(e->value) + "\"";
    if (auto* e = dynamic_cast<const IdentifierExpr*>(&expr)) return std::string(e->name);
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
        std::string op = typeMapper_.mapBinOp(e->op);
        if (e->op == BinOp::Assign) {
            return genExpr(*e->left) + " " + op + " " + genExpr(*e->right);
        }
        return "(" + genExpr(*e->left) + " " + op + " " + genExpr(*e->right) + ")";
    }
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
        return "(" + typeMapper_.mapUnOp(e->op) + genExpr(*e->operand) + ")";
    }
    if (auto* e = dynamic_cast<const CallExpr*>(&expr)) return genCallExpr(*e);
    if (auto* e = dynamic_cast<const MemberExpr*>(&expr)) {
        std::string objectStr = genExpr(*e->object);
        std::string memberStr = e->member;
        
        // Check if this is a component access in trait system (e.g., "obj.Position")
        if (inTraitSystem_) {
            auto it = componentPointers_.find(objectStr + "." + memberStr);
            if (it != componentPointers_.end()) {
                // This is a component access
                // In batch mode, return column[idx] (array element)
                // In normal mode, return pointer name
                if (inBatchSystem_) {
                    return it->second + "[__idx]";
                } else {
                    return it->second;
                }
            }
            
            // Check for nested member access (e.g., "obj.Position.x")
            // The object might already be a component pointer (normal mode) or array element (batch mode)
            // In batch mode, objectStr could be "__col_Position[__idx]" when accessing a field
            for (const auto& pair : componentPointers_) {
                // Check if objectStr starts with the component pointer name
                // Batch mode: "__col_Position[__idx]" should match "__col_Position"
                std::string colPtrName = pair.second;
                if (inBatchSystem_) {
                    // Check if objectStr is "colPtr[__idx]"
                    std::string expected = colPtrName + "[__idx]";
                    if (objectStr == expected || objectStr.find(colPtrName + "[__idx]") == 0) {
                        // This is accessing a field of a component array element
                        // Return: __col_Position[__idx].field
                        return objectStr + "." + memberStr;
                    }
                } else {
                    // Normal mode: check exact match
                    if (pair.second == objectStr) {
                        return objectStr + "->" + memberStr;
                    }
                }
            }
        }
        
        // Default: regular member access
        return objectStr + "." + memberStr;
    }
    if (auto* e = dynamic_cast<const IndexExpr*>(&expr)) return genExpr(*e->object) + "[" + genExpr(*e->index) + "]";
    if (auto* e = dynamic_cast<const FStringExpr*>(&expr)) return genFStringExpr(*e);
    if (auto* e = dynamic_cast<const HasExpr*>(&expr)) {
        // Parse component name (may have package prefix like "engine.Health")
        std::string compName = e->name;
        std::string compPkg = packageName_;
        size_t dotPos = e->name.find('.');
        if (dotPos != std::string::npos) {
            // Has package prefix: "package.ComponentName"
            compPkg = e->name.substr(0, dotPos);
            compName = e->name.substr(dotPos + 1);
        } else {
            // No package prefix, look up in dataPackageMap_
            auto it = dataPackageMap_.find(e->name);
            if (it != dataPackageMap_.end()) {
                compPkg = it->second;
            }
        }
        
        std::string compTypeId = Naming::componentTypeId(compPkg, compName);
        
        // Determine world: component belongs to its defining package's world
        // Use current world from context for local components
        std::string currentWorld = ctx_.getCurrentWorld();
        std::string worldParam = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
        if (compPkg != packageName_) {
            worldParam = "__paani_gen_" + compPkg + "_w";
        }
        
        return "paani_component_has(" + worldParam + ", " + genExpr(*e->entity) + ", " + compTypeId + ")";
    }
    if (auto* e = dynamic_cast<const SpawnExpr*>(&expr)) {
        std::string worldParam;
        std::string entityWorld;
        
        // Get current world from context (for entry point, it's __paani_gen_this_w)
        std::string currentWorld = ctx_.getCurrentWorld();
        std::string localWorld = currentWorld.empty() ? "__paani_gen_world" : currentWorld;
        
        if (!e->templateName.empty()) {
            // Template spawn: entity belongs to template's package world
            auto it = templatePackageMap_.find(e->templateName);
            if (it != templatePackageMap_.end() && it->second != packageName_) {
                // Cross-package template
                worldParam = "__paani_gen_" + it->second + "_w";
                entityWorld = worldParam;
            } else {
                // Local template or template not found (assume local)
                worldParam = localWorld;
                entityWorld = worldParam;
            }
            // Template spawn: call the generated spawn function
            return Naming::templateSpawn(packageName_, e->templateName) + "(" + worldParam + ")";
        } else {
            // Regular spawn: entity belongs to current package's world
            worldParam = localWorld;
            entityWorld = worldParam;
            return "paani_entity_create(" + worldParam + ")";
        }
    }
    if (auto* e = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
        // Array literal: generate compound literal
        // Infer element type from first element
        std::string elemType = "int32_t";  // default
        if (!e->elements.empty()) {
            elemType = inferType(*e->elements[0]);
        }
        
        std::string result = "({" + elemType + " __arr[] = {";
        for (size_t i = 0; i < e->elements.size(); i++) {
            if (i > 0) result += ", ";
            result += genExpr(*e->elements[i]);
        }
        result += "}; __arr;})";
        return result;
    }
    return "0";
}

inline std::string CodeGenerator::genFStringExpr(const FStringExpr& expr) {
    // Create type lookup function that queries varTypes_
    auto typeLookup = [this](const std::string& name) -> std::string {
        auto it = varTypes_.find(name);
        if (it != varTypes_.end()) return it->second;
        return "";
    };
    
    // Create component lookup function for trait system support
    auto componentLookup = [this](const std::string& key) -> std::string {
        if (inTraitSystem_) {
            auto it = componentPointers_.find(key);
            if (it != componentPointers_.end()) return it->second;
        }
        return "";
    };
    
    // Create field type lookup function
    auto fieldTypeLookup = [this](const std::string& key) -> std::string {
        auto it = componentFieldTypes_.find(key);
        if (it != componentFieldTypes_.end()) return it->second;
        return "";
    };
    
    // Create expression type inference function
    auto exprTypeInference = [this](const Expr& e) -> std::string {
        return inferType(e);
    };
    
    FStringGenerator fstrGen(ctx_, tempCounter_, typeLookup, componentLookup, 
                             fieldTypeLookup, exprTypeInference);
    return fstrGen.generate(expr);
}

inline std::string CodeGenerator::genCallExpr(const CallExpr& expr, bool isStmt) {
    // Case 1: Direct function call (e.g., createBody())
    if (auto* idExpr = dynamic_cast<const IdentifierExpr*>(expr.callee.get())) {
        std::string funcName = idExpr->name;
        std::string result;
        
        if (externFunctions_.count(funcName)) {
            // Extern function: no world parameter needed
            result = funcName + "(";
            for (size_t i = 0; i < expr.args.size(); i++) {
                if (i > 0) result += ", ";
                result += genExpr(*expr.args[i]);
            }
            result += ")";
        } else if (exportFunctions_.count(funcName)) {
            // Current package's export function: use current function's world parameter
            std::string worldParam = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
            result = Naming::exportFunction(packageName_, funcName) + "(" + worldParam;
            for (const auto& arg : expr.args) result += ", " + genExpr(*arg);
            result += ")";
        } else {
            // Internal function: use current function's world parameter
            std::string worldParam = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
            result = Naming::internalFunction(packageName_, funcName) + "(" + worldParam;
            for (const auto& arg : expr.args) result += ", " + genExpr(*arg);
            result += ")";
        }
        return result;
    }
    
    // Case 2: Package-qualified call (e.g., engine.createBody())
    if (auto* memberExpr = dynamic_cast<const MemberExpr*>(expr.callee.get())) {
        if (auto* pkgId = dynamic_cast<const IdentifierExpr*>(memberExpr->object.get())) {
            std::string pkgNameOrAlias = pkgId->name;
            std::string funcName = memberExpr->member;
            
            // Resolve alias to actual package name
            std::string actualPkgName = pkgNameOrAlias;
            auto aliasIt = packageAliases_.find(pkgNameOrAlias);
            if (aliasIt != packageAliases_.end()) {
                actualPkgName = aliasIt->second;
            }
            
            // Check if this is a used package
            if (usedPackages_.count(actualPkgName)) {
                // Entry point: use target package's world variable
                // Non-entry point: use current context's world
                std::string worldParam;
                if (isEntryPoint_) {
                    worldParam = Naming::worldVar(actualPkgName);
                } else {
                    worldParam = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
                }
                std::string result = Naming::exportFunction(actualPkgName, funcName) + "(" + worldParam;
                for (const auto& arg : expr.args) result += ", " + genExpr(*arg);
                result += ")";
                return result;
            } else {
                // Could be component access or other - use default behavior
                std::string worldParam = ctx_.getCurrentWorld().empty() ? "__paani_gen_world" : ctx_.getCurrentWorld();
                std::string result = Naming::exportFunction(pkgNameOrAlias, funcName) + "(" + worldParam;
                for (const auto& arg : expr.args) result += ", " + genExpr(*arg);
                result += ")";
                return result;
            }
        }
    }
    
    return "0";
}

inline std::string CodeGenerator::inferType(const Expr& expr) const {
    if (dynamic_cast<const IntegerExpr*>(&expr)) return "int32_t";
    if (dynamic_cast<const FloatExpr*>(&expr)) return "float";
    if (dynamic_cast<const BoolExpr*>(&expr)) return "int";
    if (dynamic_cast<const StringExpr*>(&expr)) return "const char*";
    if (dynamic_cast<const FStringExpr*>(&expr)) return "const char*";
    if (dynamic_cast<const SpawnExpr*>(&expr)) return "paani_entity_t";
    
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        auto it = varTypes_.find(id->name);
        if (it != varTypes_.end()) return it->second;
    }
    
    // Handle binary expressions (arithmetic, bitwise, logical)
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        switch (bin->op) {
            // Bitwise operations - return integer type
            case BinOp::BitAnd:
            case BinOp::BitOr:
            case BinOp::BitXor:
            case BinOp::BitShl:
            case BinOp::BitShr:
                return "int32_t";
            // Logical operations - return bool (int)
            case BinOp::And:
            case BinOp::Or:
                return "int";
            // Comparison operations - return bool (int)
            case BinOp::Eq:
            case BinOp::Ne:
            case BinOp::Lt:
            case BinOp::Gt:
            case BinOp::Le:
            case BinOp::Ge:
                return "int";
            // Arithmetic operations - infer from operands
            case BinOp::Add:
            case BinOp::Sub:
            case BinOp::Mul:
            case BinOp::Div:
            case BinOp::Mod: {
                std::string leftType = inferType(*bin->left);
                if (leftType == "float" || leftType == "double") return leftType;
                std::string rightType = inferType(*bin->right);
                if (rightType == "float" || rightType == "double") return rightType;
                return "int32_t";
            }
            default:
                return "int32_t";
        }
    }
    
    // Handle unary expressions
    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        switch (un->op) {
            case UnOp::Not:
                return "int";  // bool
            case UnOp::Neg:
                return inferType(*un->operand);
            default:
                return "int32_t";
        }
    }
    
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        std::string funcName;
        
        // Handle direct function call (e.g., create_player())
        if (auto* id = dynamic_cast<const IdentifierExpr*>(call->callee.get())) {
            funcName = id->name;
        }
        // Handle cross-package function call (e.g., engine.create_player())
        else if (auto* memberExpr = dynamic_cast<const MemberExpr*>(call->callee.get())) {
            if (auto* pkgId = dynamic_cast<const IdentifierExpr*>(memberExpr->object.get())) {
                // Use same naming convention as semantic analyzer: pkgName__funcName
                funcName = std::string(pkgId->name) + "__" + memberExpr->member;
            }
        }
        
        if (!funcName.empty()) {
            auto typeIt = functionReturnTypeNames_.find(funcName);
            if (typeIt != functionReturnTypeNames_.end() && typeMapper_.handleTypes.count(typeIt->second)) {
                return "void*";
            }
            auto it = functionReturnTypes_.find(funcName);
            if (it != functionReturnTypes_.end()) {
                switch (it->second) {
                    case TypeKind::Void: return "void";
                    case TypeKind::Bool: return "int";
                    case TypeKind::I32: return "int32_t";
                    case TypeKind::F32: return "float";
                    case TypeKind::F64: return "double";
                    case TypeKind::String: return "const char*";
                    case TypeKind::Entity: return "paani_entity_t";
                    default: return "int32_t";
                }
            }
        }
    }
    return "int32_t";
}

inline void CodeGenerator::registerHandleVar(const std::string& name, const std::string& typeName) {
    auto it = handleDtors_.find(typeName);
    if (it != handleDtors_.end()) {
        handleTracker_.registerHandle(name, it->second, typeName);
    }
}

inline void CodeGenerator::emitHandleDtorsForCurrentScope() {
    if (handleEmitter_) handleEmitter_->emitCurrentScope();
}

inline void CodeGenerator::emitAllHandleDtors() {
    if (handleEmitter_) handleEmitter_->emitAll();
}

inline void CodeGenerator::emitHandleMove(const std::string& varName) {
    if (handleEmitter_) handleEmitter_->emitMove(varName);
}

// Component access helper for trait systems
inline std::string CodeGenerator::genComponentAccess(const std::string& entity, const std::string& component) {
    if (inTraitSystem_) {
        auto it = componentPointers_.find(entity + "." + component);
        if (it != componentPointers_.end()) {
            return it->second + "[__i]";
        }
    }
    return "/* component access */";
}

// Get the world variable for an entity expression
inline std::string CodeGenerator::getEntityWorld(const Expr& entityExpr) {
    // Try to get entity variable name
    if (auto* idExpr = dynamic_cast<const IdentifierExpr*>(&entityExpr)) {
        auto it = entityWorlds_.find(idExpr->name);
        if (it != entityWorlds_.end()) {
            return it->second;
        }
    }
    // Default to current context's world
    std::string currentWorld = ctx_.getCurrentWorld();
    return currentWorld.empty() ? "__paani_gen_world" : currentWorld;
}

// Generate lock/unlock calls for system synchronization
inline void CodeGenerator::genLock(const LockStmt& stmt) {
    // Determine system name and world
    std::string sysName;
    std::string sysWorld;
    
    if (stmt.systemPath.size() > 1) {
        // Cross-package lock: package.system
        std::string targetPkg = stmt.systemPath[0];
        std::string sysBaseName = stmt.systemPath[1];
        sysName = Naming::system(targetPkg, sysBaseName);
        sysWorld = "__paani_gen_" + targetPkg + "_w";
    } else {
        // Local lock
        sysName = Naming::system(packageName_, stmt.systemPath[0]);
        sysWorld = ctx_.getCurrentWorld();
    }
    
    ctx_.writeImplLn(ctx_.indent() + "// Lock system: " + sysName);
    ctx_.writeImplLn(ctx_.indent() + "{");
    ctx_.pushIndent();
    ctx_.writeImplLn(ctx_.indent() + "paani_system_t __sys_lock = paani_system_find(" + sysWorld + ", \"" + sysName + "\");");
    ctx_.writeImplLn(ctx_.indent() + "if (__sys_lock != PAANI_INVALID_SYSTEM) paani_system_lock(" + sysWorld + ", __sys_lock);");
    ctx_.popIndent();
    ctx_.writeImplLn(ctx_.indent() + "}");
}

inline void CodeGenerator::genUnlock(const UnlockStmt& stmt) {
    // Determine system name and world
    std::string sysName;
    std::string sysWorld;
    
    if (stmt.systemPath.size() > 1) {
        // Cross-package unlock: package.system
        std::string targetPkg = stmt.systemPath[0];
        std::string sysBaseName = stmt.systemPath[1];
        sysName = Naming::system(targetPkg, sysBaseName);
        sysWorld = "__paani_gen_" + targetPkg + "_w";
    } else {
        // Local unlock
        sysName = Naming::system(packageName_, stmt.systemPath[0]);
        sysWorld = ctx_.getCurrentWorld();
    }
    
    ctx_.writeImplLn(ctx_.indent() + "// Unlock system: " + sysName);
    ctx_.writeImplLn(ctx_.indent() + "{");
    ctx_.pushIndent();
    ctx_.writeImplLn(ctx_.indent() + "paani_system_t __sys_unlock = paani_system_find(" + sysWorld + ", \"" + sysName + "\");");
    ctx_.writeImplLn(ctx_.indent() + "if (__sys_unlock != PAANI_INVALID_SYSTEM) paani_system_unlock(" + sysWorld + ", __sys_unlock);");
    ctx_.popIndent();
    ctx_.writeImplLn(ctx_.indent() + "}");
}

// Generate entry point main function with full game loop
inline void CodeGenerator::generateEntryMain(const Package& pkg) {
    // Forward declarations for time functions (avoid including entire time.h)
    ctx_.writeImplLn("// Forward declarations for time functions");
    ctx_.writeImplLn("typedef long long __paani_time_t;  // Use 64-bit to match timespec.tv_sec");
    ctx_.writeImplLn("struct __paani_timespec {");
    ctx_.writeImplLn("    __paani_time_t tv_sec;");
    ctx_.writeImplLn("    long tv_nsec;");
    ctx_.writeImplLn("};");
    ctx_.writeImplLn("extern int clock_gettime(int clock_id, struct __paani_timespec* tp);");
    ctx_.writeImplLn("#define CLOCK_MONOTONIC 1\n");
    
    ctx_.writeImplLn("// Entry point main function");
    ctx_.writeImplLn("int main() {");
    ctx_.pushIndent();
    
    // Time measurement
    ctx_.writeImplLn("// Time measurement for delta time");
    ctx_.writeImplLn("struct __paani_timespec __last_time, __current_time;");
    ctx_.writeImplLn("clock_gettime(CLOCK_MONOTONIC, &__last_time);");
    ctx_.writeImplLn("");
    
    // Create worlds
    for (const auto& use : pkg.uses) {
        std::string pkgName;
        for (size_t i = 0; i < use->packagePath.size(); i++) {
            if (i > 0) pkgName += "__";
            pkgName += std::string(use->packagePath[i]);
        }
        ctx_.writeImplLn("__paani_gen_" + pkgName + "_w = paani_world_create();");
    }
    ctx_.writeImplLn("__paani_gen_this_w = paani_world_create();");
    ctx_.writeImplLn("");
    
    // Initialize used packages
    for (const auto& use : pkg.uses) {
        std::string pkgName;
        for (size_t i = 0; i < use->packagePath.size(); i++) {
            if (i > 0) pkgName += "__";
            pkgName += std::string(use->packagePath[i]);
        }
        ctx_.writeImplLn(pkgName + "__paani_module_init(__paani_gen_" + pkgName + "_w);");
    }
    ctx_.writeImplLn("");
    
    // Initialize this package
    ctx_.writeImplLn("// Initialize this package");
    ctx_.writeImplLn(packageName_ + "__paani_module_init(__paani_gen_this_w);");
    ctx_.writeImplLn("");
    
    // Global statements
    if (!pkg.globalStatements.empty()) {
        ctx_.writeImplLn("// User global initialization code");
        handleTracker_.beginFunction();
        handleEmitter_ = std::make_unique<HandleEmitter>(handleTracker_, ctx_);
        
        // Set current world for global statements
        std::string savedWorld = ctx_.getCurrentWorld();
        ctx_.setCurrentWorld("__paani_gen_this_w");
        
        for (const auto& stmt : pkg.globalStatements) {
            genStmt(*stmt);
        }
        
        ctx_.setCurrentWorld(savedWorld);
        ctx_.writeImplLn("");
    }
    
    // Main game loop
    ctx_.writeImplLn("// Main game loop");
    ctx_.writeImplLn("int running = 1;");
    ctx_.writeImplLn("while (running) {");
    ctx_.pushIndent();
        ctx_.writeImplLn("    // Calculate delta time (milliseconds)");
        ctx_.writeImplLn("    clock_gettime(CLOCK_MONOTONIC, &__current_time);");
        ctx_.writeImplLn("    __paani_time_t sec_diff = __current_time.tv_sec - __last_time.tv_sec;");
        ctx_.writeImplLn("    long nsec_diff = __current_time.tv_nsec - __last_time.tv_nsec;");
        ctx_.writeImplLn("    if (nsec_diff < 0) {");
        ctx_.pushIndent();
        ctx_.writeImplLn("sec_diff--;");
        ctx_.writeImplLn("nsec_diff += 1000000000L;");
        ctx_.popIndent();
        ctx_.writeImplLn("}");
        ctx_.writeImplLn("    // Convert to milliseconds: sec * 1000 + nsec / 1000000");
        ctx_.writeImplLn("    float dt = (float)sec_diff * 1000.0f + (float)nsec_diff / 1000000.0f;");
        ctx_.writeImplLn("    __last_time = __current_time;");
        ctx_.writeImplLn("    if (dt > 100.0f) dt = 100.0f; // Clamp to prevent large time steps (100ms max)");
    ctx_.writeImplLn("");
    
    // Run systems for each world
    for (const auto& use : pkg.uses) {
        std::string pkgName;
        for (size_t i = 0; i < use->packagePath.size(); i++) {
            if (i > 0) pkgName += "__";
            pkgName += std::string(use->packagePath[i]);
        }
        ctx_.writeImplLn("paani_system_run_all(__paani_gen_" + pkgName + "_w, dt);");
    }
    ctx_.writeImplLn("paani_system_run_all(__paani_gen_this_w, dt);");
    ctx_.writeImplLn("");
    
    // Check exit flag
    ctx_.writeImplLn("// Check exit flag (set by 'exit' statement)");
    ctx_.writeImplLn("if (paani_should_exit(__paani_gen_this_w)) {");
    ctx_.pushIndent();
    ctx_.writeImplLn("running = 0;");
    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.popIndent();
    ctx_.writeImplLn("}");
    ctx_.writeImplLn("");
    
    // Cleanup
    ctx_.writeImplLn("// Cleanup");
    if (!pkg.globalStatements.empty()) {
        ctx_.writeImplLn("// Global handle cleanup");
        emitAllHandleDtors();
    }
    
    for (const auto& use : pkg.uses) {
        std::string pkgName;
        for (size_t i = 0; i < use->packagePath.size(); i++) {
            if (i > 0) pkgName += "__";
            pkgName += std::string(use->packagePath[i]);
        }
        ctx_.writeImplLn("paani_world_destroy(__paani_gen_" + pkgName + "_w);");
    }
    ctx_.writeImplLn("paani_world_destroy(__paani_gen_this_w);");
    ctx_.writeImplLn("return 0;");
    ctx_.popIndent();
    ctx_.writeImplLn("}");
}

} // namespace paani

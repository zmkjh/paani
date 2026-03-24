// Naming.hpp - Unified naming conventions for code generation
#pragma once

#include <string>
#include <vector>
#include <unordered_set>

namespace paani {

// Symbol categories for code generation
enum class SymbolCategory {
    InternalFunction,   // Internal package function (no prefix)
    ExportFunction,     // Exported function (package__ prefix)
    ExternFunction,     // External C function (no prefix, direct call)
    Component,          // Data component type (package__ prefix)
    Trait,              // Trait type (package__ prefix)
    System,             // System name for registration
    SystemFunction,     // System implementation function
    TemplateSpawn,      // Template spawn function
    ComponentTypeId,    // Component type ID variable
    TraitTypeId,        // Trait type ID variable
    WorldVar,           // Package world variable
    ModuleInit,         // Module initialization function
    ExternType,         // External type (package__ prefix)
    HandleType,         // Handle type (package__ prefix)
};

// Unified naming utility for all code generation
class Naming {
public:
    // ========== Core Naming Functions ==========
    
    // Generate symbol name based on category
    static std::string symbol(SymbolCategory cat, 
                              const std::string& package, 
                              const std::string& name);
    
    // ========== Component Naming ==========
    static std::string component(const std::string& package, const std::string& name);
    static std::string componentTypeId(const std::string& package, const std::string& name);
    
    // ========== Trait Naming ==========
    static std::string trait(const std::string& package, const std::string& name);
    static std::string traitTypeId(const std::string& package, const std::string& name);
    
    // ========== System Naming ==========
    static std::string system(const std::string& package, const std::string& name);
    static std::string systemFunction(const std::string& package, const std::string& name);
    
    // ========== Function Naming ==========
    // Internal function: package__ prefix (all functions are prefixed)
    static std::string internalFunction(const std::string& package, const std::string& name);
    // Export function: package__ prefix
    static std::string exportFunction(const std::string& package, const std::string& name);
    // Extern function: no prefix (direct C call)
    static std::string externFunction(const std::string& name);
    // Generic function dispatcher
    static std::string function(const std::string& package, const std::string& name, 
                                bool isExport, bool isExtern);
    
    // ========== Template Naming ==========
    static std::string templateSpawn(const std::string& package, const std::string& name);
    
    // ========== Module & World Naming ==========
    static std::string moduleInit(const std::string& package);
    static std::string worldVar(const std::string& package);
    static std::string thisWorldVar();
    
    // ========== Extern & Handle Types ==========
    static std::string externType(const std::string& package, const std::string& name);
    static std::string handleType(const std::string& package, const std::string& name);
    
    // ========== Generic Helpers ==========
    static std::string withPackage(const std::string& package, const std::string& name);
    static std::string withPrefix(const std::string& prefix, const std::string& name);
    
    // ========== Reserved Names Checking ==========
    static bool isReserved(const std::string& name);
    static bool isCReserved(const std::string& name);
    static bool isCompilerReserved(const std::string& name);
    static std::string suggestAlternative(const std::string& name);
    
    // ========== C Standard Library Header Lookup ==========
    // Get the C header file that declares the given standard library function
    static std::string getCHeaderForFunction(const std::string& name);
    
private:
    static const std::unordered_set<std::string> C_RESERVED_NAMES;
    static const std::unordered_set<std::string> C_RESERVED_PREFIXES;
    static const std::unordered_set<std::string> COMPILER_RESERVED_PREFIXES;
};

} // namespace paani

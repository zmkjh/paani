// paani_package.hpp - Package Manager for Paani
// Handles package search, loading, and symbol resolution

#ifndef PAANI_PACKAGE_HPP
#define PAANI_PACKAGE_HPP

#include "paani_ast.hpp"
#include "paani_parser.hpp"
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>

namespace paani {

// ============================================================================
// Package Info
// ============================================================================
struct PackageInfo {
    std::string name;
    std::filesystem::path path;
    std::unique_ptr<Package> ast;

    // Exported symbols
    std::unordered_map<std::string, TypePtr> exportedFunctions;
    std::unordered_map<std::string, TypePtr> exportedDatas;           // export data
    std::unordered_map<std::string, TypePtr> exportedTraits;          // export trait
    std::unordered_map<std::string, size_t> exportedTemplates;        // export template -> component count
    std::unordered_map<std::string, std::string> exportedHandles;     // export handle -> dtor function
    std::unordered_map<std::string, TypePtr> exportedExternFunctions; // extern fn + export
    std::unordered_map<std::string, uint32_t> exportedExternTypes;    // extern type + export -> size

    PackageInfo(const std::string& n, const std::filesystem::path& p)
        : name(n), path(p) {}
};

// ============================================================================
// Package Manager
// ============================================================================
class PackageManager {
public:
    // Search paths for packages
    std::vector<std::filesystem::path> searchPaths;

    // Loaded packages
    std::unordered_map<std::string, std::unique_ptr<PackageInfo>> loadedPackages;

    PackageManager() {
        // Add default search paths
        // 1. Current directory
        searchPaths.push_back(std::filesystem::current_path());
    }

    // Add a custom search path (e.g., for -I or --include-dir option)
    void addSearchPath(const std::string& path) {
        searchPaths.push_back(path);
    }

    // Load a package for use declaration
    // onlyUtils = true: 只加载 utils/ 子目录（非入口文件的 use）
    // onlyUtils = false: 加载整个包（入口文件的 use）
    PackageInfo* loadPackageForUse(const std::vector<std::string>& packagePath, bool onlyUtils) {
        if (packagePath.empty()) return nullptr;

        std::string packageName = packagePath[0];
        for (size_t i = 1; i < packagePath.size(); i++) {
            packageName += "__" + packagePath[i];
        }

        // Build cache key that includes onlyUtils flag
        // This ensures utils-only and full package are cached separately
        std::string cacheKey = packageName + (onlyUtils ? "#utils" : "#full");

        // Check if already loaded
        auto it = loadedPackages.find(cacheKey);
        if (it != loadedPackages.end()) {
            return it->second.get();
        }

        // Search for the package directory
        std::filesystem::path dirPath = findPackageDirectory(packagePath);
        if (dirPath.empty()) {
            return nullptr;
        }

        // Load and parse the package (merge all .paani files)
        auto pkgInfo = std::make_unique<PackageInfo>(packageName, dirPath);

        // Merge .paani files - only utils/ if onlyUtils is true
        std::string mergedSource = mergePackageFiles(dirPath, onlyUtils);
        if (mergedSource.empty()) {
            return nullptr;
        }

        Parser parser(mergedSource, packageName);
        pkgInfo->ast = parser.parse();

        if (!pkgInfo->ast) {
            return nullptr;
        }

        // Check for ECS code in utils-only mode
        if (onlyUtils && hasECSCode(*pkgInfo->ast)) {
            throw std::runtime_error("Package '" + packageName + ": utils/ directory contains ECS code (data/trait/system), which is not allowed");
        }

        // Collect exported symbols
        collectExportedSymbols(*pkgInfo);

        auto* result = pkgInfo.get();
        loadedPackages[cacheKey] = std::move(pkgInfo);
        return result;
    }

    // Load a package by its path (e.g., "engine" -> "engine/" directory)
    // Merges all .paani files in the directory (full package)
    PackageInfo* loadPackage(const std::vector<std::string>& packagePath) {
        return loadPackageForUse(packagePath, false);
    }

    // Get exported function type from a package
    Type* getExportedFunctionType(const std::vector<std::string>& packagePath,
                                  const std::vector<std::string>& funcPath) {
        PackageInfo* pkg = loadPackage(packagePath);
        if (!pkg) return nullptr;

        // Build full function name (e.g., "math__clamp")
        std::string funcName;
        for (size_t i = 0; i < funcPath.size(); i++) {
            if (i > 0) funcName += "__";
            funcName += funcPath[i];
        }

        auto it = pkg->exportedFunctions.find(funcName);
        if (it != pkg->exportedFunctions.end()) {
            return it->second.get();
        }

        return nullptr;
    }

private:
    std::string readFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::filesystem::path findPackageFile(const std::vector<std::string>& packagePath) {
        // Build relative path (e.g., "std/math.paani")
        std::filesystem::path relPath;
        for (size_t i = 0; i < packagePath.size(); i++) {
            if (i > 0) relPath /= "";
            if (i == packagePath.size() - 1) {
                relPath /= packagePath[i] + ".paani";
            } else {
                relPath /= packagePath[i];
            }
        }

        // Search in all search paths
        for (const auto& searchPath : searchPaths) {
            std::filesystem::path fullPath = searchPath / relPath;
            if (std::filesystem::exists(fullPath)) {
                return fullPath;
            }
        }

        return {};
    }

    // Find package directory (e.g., "engine" -> "engine/")
    std::filesystem::path findPackageDirectory(const std::vector<std::string>& packagePath) {
        if (packagePath.empty()) return {};

        // Build directory path (e.g., "engine")
        std::filesystem::path dirPath = packagePath[0];
        for (size_t i = 1; i < packagePath.size(); i++) {
            dirPath /= packagePath[i];
        }

        // Search in all search paths
        for (const auto& searchPath : searchPaths) {
            std::filesystem::path fullPath = searchPath / dirPath;
            if (std::filesystem::exists(fullPath) && std::filesystem::is_directory(fullPath)) {
                return fullPath;
            }
        }

        return {};
    }

    // Merge all .paani files in a directory
    // When onlyUtils is true, only loads files from utils/ subdirectory
    std::string mergePackageFiles(const std::filesystem::path& dirPath, bool onlyUtils = false) {
        std::string mergedSource;
        bool foundFiles = false;

        // If onlyUtils, only look in utils/ subdirectory
        std::filesystem::path searchPath = dirPath;
        if (onlyUtils) {
            searchPath = dirPath / "utils";
            if (!std::filesystem::exists(searchPath)) {
                // utils/ directory doesn't exist, return empty
                return "";
            }
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(searchPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".paani") {
                foundFiles = true;
                std::string content = readFile(entry.path().string());
                mergedSource += "// ===== " + entry.path().filename().string() + " =====\n";
                mergedSource += content;
                mergedSource += "\n\n";
            }
        }

        return foundFiles ? mergedSource : "";
    }

    // Check if package contains ECS code (data, trait, system)
    bool hasECSCode(const Package& pkg) {
        return !pkg.datas.empty() || !pkg.traits.empty() || !pkg.systems.empty();
    }

    void collectExportedSymbols(PackageInfo& pkgInfo) {
        if (!pkgInfo.ast) return;

        // Collect exported functions
        for (const auto& fn : pkgInfo.ast->functions) {
            if (fn->isExport) {
                std::string fullName = fn->name;

                // Build function type
                auto funcType = std::make_unique<Type>(TypeKind::Function);
                if (fn->returnType) {
                    funcType->funcReturn = Type::userDefined(fn->returnType->toString());
                } else {
                    funcType->funcReturn = Type::void_();
                }
                
                // Add implicit 'world' parameter for all export functions
                funcType->funcParams.push_back(Type::pointer(Type::void_()));
                
                // Add user-defined parameters (correctly copy the type)
                for (const auto& param : fn->params) {
                    if (param.type) {
                        // Create a proper copy of the type based on its kind
                        TypePtr paramType;
                        switch (param.type->kind) {
                            case TypeKind::Void: paramType = Type::void_(); break;
                            case TypeKind::Bool: paramType = Type::bool_(); break;
                            case TypeKind::I8: paramType = std::make_unique<Type>(TypeKind::I8); break;
                            case TypeKind::I16: paramType = std::make_unique<Type>(TypeKind::I16); break;
                            case TypeKind::I32: paramType = std::make_unique<Type>(TypeKind::I32); break;
                            case TypeKind::I64: paramType = std::make_unique<Type>(TypeKind::I64); break;
                            case TypeKind::U8: paramType = std::make_unique<Type>(TypeKind::U8); break;
                            case TypeKind::U16: paramType = std::make_unique<Type>(TypeKind::U16); break;
                            case TypeKind::U32: paramType = std::make_unique<Type>(TypeKind::U32); break;
                            case TypeKind::U64: paramType = std::make_unique<Type>(TypeKind::U64); break;
                            case TypeKind::F32: paramType = std::make_unique<Type>(TypeKind::F32); break;
                            case TypeKind::F64: paramType = std::make_unique<Type>(TypeKind::F64); break;
                            case TypeKind::String: paramType = Type::string(); break;
                            case TypeKind::Entity: paramType = Type::entity(); break;
                            case TypeKind::Pointer: 
                                if (param.type->base) {
                                    paramType = Type::pointer(std::make_unique<Type>(param.type->base->kind));
                                } else {
                                    paramType = Type::pointer(Type::void_());
                                }
                                break;
                            case TypeKind::UserDefined:
                                paramType = Type::userDefined(param.type->name);
                                break;
                            default:
                                paramType = Type::void_();
                                break;
                        }
                        funcType->funcParams.push_back(std::move(paramType));
                    }
                }

                pkgInfo.exportedFunctions[fullName] = std::move(funcType);
            }
        }

        // Collect exported data types only
        for (const auto& data : pkgInfo.ast->datas) {
            if (data->isExport) {
                pkgInfo.exportedDatas[data->name] = Type::userDefined(data->name);
            }
        }

        // Collect exported trait types only
        for (const auto& trait : pkgInfo.ast->traits) {
            if (trait->isExport) {
                pkgInfo.exportedTraits[trait->name] = Type::userDefined(trait->name);
            }
        }
        
        // Collect exported templates only
        for (const auto& tmpl : pkgInfo.ast->templates) {
            if (tmpl->isExport) {
                // Store template info for cross-package access
                // TemplateInfo structure can be extended if needed
                pkgInfo.exportedTemplates[tmpl->name] = tmpl->components.size();
            }
        }
        
        // Collect exported handles only
        for (const auto& handle : pkgInfo.ast->handles) {
            if (handle->isExport) {
                // Store handle info: name -> destructor function
                pkgInfo.exportedHandles[handle->name] = handle->dtor;
            }
        }
        
        // Note: extern functions are NOT collected for export
        // They are private to the package that declares them and cannot be accessed via 'use'
        // This is intentional - extern functions bind to specific C code and should not be shared across packages
        
        // Collect extern types
        for (const auto& extType : pkgInfo.ast->externTypes) {
            std::string fullName = pkgInfo.name + "__" + extType->name;
            pkgInfo.exportedExternTypes[fullName] = extType->size;
        }
    }
};

} // namespace paani

#endif // PAANI_PACKAGE_HPP

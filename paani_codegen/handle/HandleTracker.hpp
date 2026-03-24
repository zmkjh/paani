// HandleTracker.hpp - Handle 变量跟踪
// 管理 handle 变量的生命周期和 dtor 调用

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace paani {

// Handle variable info: (varName, dtorFunc)
using HandleInfo = std::pair<std::string, std::string>;

class HandleTracker {
public:
    // Scope stack: each scope is a list of handle variables
    std::vector<std::vector<HandleInfo>> scopeStack;
    
    // Map: varName -> handleTypeName
    std::unordered_map<std::string, std::string> varTypes;
    
    // Set of moved handles (should skip dtor)
    std::unordered_set<std::string> movedHandles;
    
public:
    // Initialize new function
    void beginFunction() {
        scopeStack.clear();
        scopeStack.emplace_back();  // Root scope
        varTypes.clear();
        movedHandles.clear();
    }
    
    // End function (cleanup)
    void endFunction() {
        scopeStack.clear();
    }
    
    // Push new scope
    void pushScope() {
        scopeStack.emplace_back();
    }
    
    // Pop scope and return handle list for dtor generation
    std::vector<HandleInfo> popScope() {
        if (scopeStack.empty()) return {};
        auto handles = std::move(scopeStack.back());
        scopeStack.pop_back();
        return handles;
    }
    
    // Register a handle variable
    void registerHandle(const std::string& varName, const std::string& dtorFunc, 
                        const std::string& typeName) {
        if (!scopeStack.empty()) {
            scopeStack.back().push_back({varName, dtorFunc});
            varTypes[varName] = typeName;
        }
    }
    
    // Check if variable is a handle
    bool isHandle(const std::string& varName) const {
        return varTypes.count(varName) > 0;
    }
    
    // Mark handle as moved
    void markMoved(const std::string& varName) {
        movedHandles.insert(varName);
    }
    
    // Check if handle was moved
    bool isMoved(const std::string& varName) const {
        return movedHandles.count(varName) > 0;
    }
    
    // Get dtor for handle variable
    std::string getDtor(const std::string& varName) const {
        for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
            for (const auto& handle : *it) {
                if (handle.first == varName) {
                    return handle.second;
                }
            }
        }
        return "";
    }
    
    // Get all handles in current scope (for dtor generation)
    const std::vector<HandleInfo>& currentScope() const {
        static std::vector<HandleInfo> empty;
        return scopeStack.empty() ? empty : scopeStack.back();
    }
    
    // Check if tracker is active (inside a function)
    bool isActive() const {
        return !scopeStack.empty();
    }
};

} // namespace paani

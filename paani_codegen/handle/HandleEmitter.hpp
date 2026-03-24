// HandleEmitter.hpp - Handle dtor 代码生成
// 生成 handle 变量的 dtor 调用代码

#pragma once
#include "HandleTracker.hpp"
#include "../core/CodeGenContext.hpp"
#include <unordered_set>

namespace paani {

class HandleEmitter {
private:
    HandleTracker& tracker_;
    CodeGenContext& ctx_;
    
public:
    HandleEmitter(HandleTracker& tracker, CodeGenContext& ctx)
        : tracker_(tracker), ctx_(ctx) {}
    
    // Emit dtors for current scope (for block end)
    void emitCurrentScope() {
        if (!tracker_.isActive()) return;
        
        const auto& handles = tracker_.currentScope();
        // Call dtors in reverse order (LIFO)
        for (auto it = handles.rbegin(); it != handles.rend(); ++it) {
            if (!tracker_.isMoved(it->first)) {
                emitDtor(it->first, it->second);
            }
        }
    }
    
    // Emit dtors for all scopes (for return/exit)
    void emitAll() {
        if (!tracker_.isActive()) return;
        
        // Track which handles we've already emitted dtors for in this call
        std::unordered_set<std::string> emitted;
        
        // Call dtors from innermost to outermost
        for (auto scopeIt = tracker_.scopeStack.rbegin(); 
             scopeIt != tracker_.scopeStack.rend(); ++scopeIt) {
            for (auto varIt = scopeIt->rbegin(); varIt != scopeIt->rend(); ++varIt) {
                // Only emit once per handle, even if appears in multiple scopes
                if (emitted.insert(varIt->first).second) {
                    emitDtor(varIt->first, varIt->second);
                }
            }
        }
    }
    
    // Emit single dtor call with NULL check, then set to NULL
    void emitDtor(const std::string& varName, const std::string& dtorFunc) {
        ctx_.writeImpl(ctx_.indent() + "if (" + varName + " != NULL) { " + 
                       dtorFunc + "(" + varName + "); " + varName + " = NULL; }\n");
    }
    
    // Emit move: var = NULL
    void emitMove(const std::string& varName) {
        ctx_.writeImpl(ctx_.indent() + varName + " = NULL;\n");
        tracker_.markMoved(varName);
    }
    
    // Emit assignment with move semantics
    void emitAssignWithMove(const std::string& lhs, const std::string& rhs) {
        ctx_.writeImpl(ctx_.indent() + lhs + " = " + rhs + ";\n");
        emitMove(rhs);
    }
};

} // namespace paani

// CodeGenContext.hpp - 代码生成上下文
// 管理 header 和 implementation 的输出

#pragma once
#include <sstream>
#include <string>

namespace paani {

class CodeGenContext {
public:
    std::ostringstream header;
    std::ostringstream impl;
    
private:
    int indentLevel_ = 0;
    std::string currentWorld_;
    
public:
    // Indentation management
    void pushIndent() { indentLevel_++; }
    void popIndent() { if (indentLevel_ > 0) indentLevel_--; }
    std::string indent() const { return std::string(indentLevel_ * 4, ' '); }
    
    // World context for ECS calls
    void setCurrentWorld(const std::string& world) { currentWorld_ = world; }
    std::string getCurrentWorld() const { return currentWorld_; }
    
    // Write helpers
    void writeHeader(const std::string& text) { header << text; }
    void writeImpl(const std::string& text) { impl << text; }
    void writeHeaderLn(const std::string& text) { header << text << "\n"; }
    void writeImplLn(const std::string& text) { impl << text << "\n"; }
};

} // namespace paani

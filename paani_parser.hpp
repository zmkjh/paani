// paani_parser.hpp - Paani Parser
// Recursive descent parser with clear error handling

#ifndef PAANI_PARSER_HPP
#define PAANI_PARSER_HPP

#include "paani_lexer.hpp"
#include "paani_colors.hpp"
#include <functional>
#include <vector>
#include <stdexcept>
#include <iostream>

namespace paani {

// ============================================================================
// Parser Error
// ============================================================================
struct ParseError : public std::runtime_error {
    Location loc;

    ParseError(Location l, const std::string& msg)
        : std::runtime_error("Parse error at " + l.toString() + ": " + msg),
          loc(l) {}
};

// ============================================================================
// Parser
// ============================================================================
class Parser {
public:
    Parser(std::string_view source, std::string packageName = "main");

    // Parse entire source file into a Package
    std::unique_ptr<Package> parse();

    // Parse a specific declaration (for incremental parsing)
    std::unique_ptr<Decl> parseDeclaration();

    // Error recovery - skip to next statement/declaration boundary
    void synchronize();
    
    // Set whether this is an entry point file (allows global statements)
    void setEntryPoint(bool isEntry) { isEntryPoint_ = isEntry; }
    bool isEntryPoint() const { return isEntryPoint_; }

private:
    Lexer lexer_;
    std::string packageName_;
    bool isEntryPoint_ = false;

    // Current token access
    const Token& current() const { return lexer_.current(); }
    TokenType peek() const { return lexer_.current().type; }

    // Token matching and consumption
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);
    Token advance();
    bool check(TokenType type) const;

    // Check if at end
    bool isAtEnd() const { return peek() == TokenType::End; }

    // Helper for parsing lists
    template<typename T>
    std::vector<T> parseList(std::function<T()> parseItem, TokenType delimiter);

    // =========================================================================
    // Type Parsing
    // =========================================================================
    TypePtr parseType();

    // =========================================================================
    // Expression Parsing
    // =========================================================================
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    // Bitwise operators (precedence between logical and equality)
    ExprPtr parseBitwiseOr();
    ExprPtr parseBitwiseXor();
    ExprPtr parseBitwiseAnd();
    ExprPtr parseBitwiseShift();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    ExprPtr parseSpawnExpr();
    ExprPtr parseFString();
    ExprPtr parseHasExpr();

    // Helper functions
    std::string unescapeBraces(const std::string& s);

    // =========================================================================
    // Statement Parsing
    // =========================================================================
    StmtPtr parseStatement();
    StmtPtr parseBlock();
    StmtPtr parseVarDecl();
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseReturnStmt();
    StmtPtr parseBreakStmt();
    StmtPtr parseContinueStmt();
    StmtPtr parseExitStmt();
    StmtPtr parseGiveStmt();
    StmtPtr parseTagStmt();
    StmtPtr parseUntagStmt();
    StmtPtr parseDestroyStmt();
    StmtPtr parseQueryStmt();
    StmtPtr parseLockStmt();
    StmtPtr parseUnlockStmt();
    StmtPtr parseExprStmt();

    // =========================================================================
    // Declaration Parsing
    // =========================================================================
    std::unique_ptr<UseDecl> parseUseDecl();
    std::unique_ptr<ExportDecl> parseExportDecl();
    std::unique_ptr<ExportDecl> parseExportDeclAfterConsume();
    std::unique_ptr<ExternFnDecl> parseExternFnDecl();
    std::unique_ptr<ExternTypeDecl> parseExternTypeDecl();
    std::unique_ptr<HandleDecl> parseHandleDecl(bool exported = false);
    std::unique_ptr<DataDecl> parseDataDecl(bool exported = false);
    std::unique_ptr<TraitDecl> parseTraitDecl(bool exported = false);
    std::unique_ptr<TemplateDecl> parseTemplateDecl(bool exported = false);
    std::unique_ptr<SystemDecl> parseSystemDecl(bool isBatch = false);
    std::unique_ptr<SystemDecl> parseForTraitSystem(bool isBatch = false);
    std::unique_ptr<DependencyDecl> parseDependencyDecl();
    std::unique_ptr<FnDecl> parseFnDecl(bool alreadyConsumedExport = false);

    // =========================================================================
    // Helper Parsing
    // =========================================================================
    Field parseField();
    Param parseParam();
    std::vector<std::pair<std::string, ExprPtr>> parseFieldInits();
};

// ============================================================================
// Implementations
// ============================================================================

inline Parser::Parser(std::string_view source, std::string packageName)
    : lexer_(source), packageName_(std::move(packageName)) {}

inline bool Parser::match(TokenType type) {
    if (peek() == type) {
        advance();
        return true;
    }
    return false;
}

inline bool Parser::match(std::initializer_list<TokenType> types) {
    for (auto type : types) {
        if (match(type)) return true;
    }
    return false;
}

inline Token Parser::consume(TokenType type, const std::string& message) {
    if (peek() == type) {
        return advance();
    }
    throw ParseError(current().loc, message);
}

inline Token Parser::advance() {
    Token prev = lexer_.current();
    lexer_.next();
    return prev;
}

inline void Parser::synchronize() {
    advance();

    while (!isAtEnd()) {
        // Skip to next declaration boundary
        switch (peek()) {
            case TokenType::Handle:
            case TokenType::Data:
            case TokenType::Trait:
            case TokenType::Template:
            case TokenType::System:
            case TokenType::Depend:
            case TokenType::Export:
            case TokenType::Fn:
            case TokenType::For:
                return;
            default:
                break;
        }
        advance();
    }
}

// Type parsing
inline TypePtr Parser::parseType() {
    Location loc = current().loc;

    switch (peek()) {
        case TokenType::Void:
            advance();
            return Type::void_();
        case TokenType::Bool:
            advance();
            return Type::bool_();
        case TokenType::I8: advance(); return std::make_unique<Type>(TypeKind::I8);
        case TokenType::I16: advance(); return std::make_unique<Type>(TypeKind::I16);
        case TokenType::I32: advance(); return std::make_unique<Type>(TypeKind::I32);
        case TokenType::I64: advance(); return std::make_unique<Type>(TypeKind::I64);
        case TokenType::U8: advance(); return std::make_unique<Type>(TypeKind::U8);
        case TokenType::U16: advance(); return std::make_unique<Type>(TypeKind::U16);
        case TokenType::U32: advance(); return std::make_unique<Type>(TypeKind::U32);
        case TokenType::U64: advance(); return std::make_unique<Type>(TypeKind::U64);
        case TokenType::F32: advance(); return std::make_unique<Type>(TypeKind::F32);
        case TokenType::F64: advance(); return std::make_unique<Type>(TypeKind::F64);
        case TokenType::String:
            advance();
            return Type::string();
        case TokenType::Entity:
            advance();
            return Type::entity();
        case TokenType::Identifier: {
            std::string name = std::string(advance().text);
            // Check for package-qualified type name (e.g., "engine.Window")
            while (match(TokenType::Dot)) {
                if (!check(TokenType::Identifier)) {
                    throw ParseError(current().loc, "Expected type name after '.'");
                }
                name += "__" + std::string(advance().text);
            }
            return Type::userDefined(name);
        }
        case TokenType::LBracket: {
            advance(); // consume '['
            TypePtr baseType = parseType();
            consume(TokenType::Semicolon, "Expected ';' after array element type");
            if (!check(TokenType::IntegerLiteral)) {
                throw ParseError(current().loc, "Expected array size (integer literal)");
            }
            uint32_t size = static_cast<uint32_t>(std::stoul(std::string(advance().text)));
            consume(TokenType::RBracket, "Expected ']' after array size");
            return Type::array(std::move(baseType), size);
        }
        default:
            throw ParseError(loc, "Expected type");
    }
}

// Expression parsing (precedence climbing)
inline ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

inline ExprPtr Parser::parseAssignment() {
    ExprPtr expr = parseLogicalOr();

    if (match(TokenType::Assign)) {
        Token op = lexer_.current();
        ExprPtr right = parseAssignment();
        return std::make_unique<BinaryExpr>(op.loc, BinOp::Assign,
                                            std::move(expr), std::move(right));
    }
    if (match(TokenType::AddAssign)) {
        Token op = lexer_.current();
        ExprPtr right = parseAssignment();
        return std::make_unique<BinaryExpr>(op.loc, BinOp::AddAssign,
                                            std::move(expr), std::move(right));
    }
    if (match(TokenType::SubAssign)) {
        Token op = lexer_.current();
        ExprPtr right = parseAssignment();
        return std::make_unique<BinaryExpr>(op.loc, BinOp::SubAssign,
                                            std::move(expr), std::move(right));
    }

    return expr;
}

inline ExprPtr Parser::parseLogicalOr() {
    ExprPtr left = parseLogicalAnd();

    while (match(TokenType::Or)) {
        Token op = lexer_.current();
        ExprPtr right = parseLogicalAnd();
        left = std::make_unique<BinaryExpr>(op.loc, BinOp::Or,
                                            std::move(left), std::move(right));
    }

    return left;
}

inline ExprPtr Parser::parseLogicalAnd() {
    ExprPtr left = parseBitwiseOr();

    while (match(TokenType::And)) {
        Token op = lexer_.current();
        ExprPtr right = parseBitwiseOr();
        left = std::make_unique<BinaryExpr>(op.loc, BinOp::And,
                                            std::move(left), std::move(right));
    }

    return left;
}

// Bitwise OR (lowest precedence among bitwise operators)
inline ExprPtr Parser::parseBitwiseOr() {
    ExprPtr left = parseBitwiseXor();

    while (match(TokenType::BitOr)) {
        Token op = lexer_.current();
        ExprPtr right = parseBitwiseXor();
        left = std::make_unique<BinaryExpr>(op.loc, BinOp::BitOr,
                                            std::move(left), std::move(right));
    }

    return left;
}

// Bitwise XOR
inline ExprPtr Parser::parseBitwiseXor() {
    ExprPtr left = parseBitwiseAnd();

    while (match(TokenType::BitXor)) {
        Token op = lexer_.current();
        ExprPtr right = parseBitwiseAnd();
        left = std::make_unique<BinaryExpr>(op.loc, BinOp::BitXor,
                                            std::move(left), std::move(right));
    }

    return left;
}

// Bitwise AND
inline ExprPtr Parser::parseBitwiseAnd() {
    ExprPtr left = parseBitwiseShift();

    while (match(TokenType::BitAnd)) {
        Token op = lexer_.current();
        ExprPtr right = parseBitwiseShift();
        left = std::make_unique<BinaryExpr>(op.loc, BinOp::BitAnd,
                                            std::move(left), std::move(right));
    }

    return left;
}

// Bitwise shift (<<, >>)
inline ExprPtr Parser::parseBitwiseShift() {
    ExprPtr left = parseEquality();

    while (true) {
        if (match(TokenType::BitShl)) {
            Token op = lexer_.current();
            ExprPtr right = parseEquality();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::BitShl,
                                                std::move(left), std::move(right));
        } else if (match(TokenType::BitShr)) {
            Token op = lexer_.current();
            ExprPtr right = parseEquality();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::BitShr,
                                                std::move(left), std::move(right));
        } else {
            break;
        }
    }

    return left;
}

inline ExprPtr Parser::parseEquality() {
    ExprPtr left = parseComparison();

    while (true) {
        if (match(TokenType::Eq)) {
            Token op = lexer_.current();
            ExprPtr right = parseComparison();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Eq,
                                                std::move(left), std::move(right));
        } else if (match(TokenType::Ne)) {
            Token op = lexer_.current();
            ExprPtr right = parseComparison();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Ne,
                                                std::move(left), std::move(right));
        } else {
            break;
        }
    }

    return left;
}

inline ExprPtr Parser::parseComparison() {
    ExprPtr left = parseAdditive();

    while (true) {
        if (match(TokenType::Lt)) {
            Token op = lexer_.current();
            ExprPtr right = parseAdditive();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Lt,
                                                std::move(left), std::move(right));
        } else if (match(TokenType::Le)) {
            Token op = lexer_.current();
            ExprPtr right = parseAdditive();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Le,
                                                std::move(left), std::move(right));
        } else if (match(TokenType::Gt)) {
            Token op = lexer_.current();
            ExprPtr right = parseAdditive();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Gt,
                                                std::move(left), std::move(right));
        } else if (match(TokenType::Ge)) {
            Token op = lexer_.current();
            ExprPtr right = parseAdditive();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Ge,
                                                std::move(left), std::move(right));
        } else {
            break;
        }
    }

    return left;
}

inline ExprPtr Parser::parseAdditive() {
    ExprPtr left = parseMultiplicative();

    while (true) {
        if (match(TokenType::Add)) {
            Token op = lexer_.current();
            ExprPtr right = parseMultiplicative();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Add,
                                                std::move(left), std::move(right));
        } else if (match(TokenType::Sub)) {
            Token op = lexer_.current();
            ExprPtr right = parseMultiplicative();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Sub,
                                                std::move(left), std::move(right));
        } else {
            break;
        }
    }

    return left;
}

inline ExprPtr Parser::parseMultiplicative() {
    ExprPtr left = parseUnary();

    while (true) {
        if (match(TokenType::Mul)) {
            Token op = lexer_.current();
            ExprPtr right = parseUnary();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Mul,
                                                std::move(left), std::move(right));
        } else if (match(TokenType::Div)) {
            Token op = lexer_.current();
            ExprPtr right = parseUnary();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Div,
                                                std::move(left), std::move(right));
        } else if (match(TokenType::Mod)) {
            Token op = lexer_.current();
            ExprPtr right = parseUnary();
            left = std::make_unique<BinaryExpr>(op.loc, BinOp::Mod,
                                                std::move(left), std::move(right));
        } else {
            break;
        }
    }

    return left;
}

inline ExprPtr Parser::parseUnary() {
    if (match(TokenType::Sub)) {
        Token op = lexer_.current();
        ExprPtr operand = parseUnary();
        return std::make_unique<UnaryExpr>(op.loc, UnOp::Neg, std::move(operand));
    }
    if (match(TokenType::Not)) {
        Token op = lexer_.current();
        ExprPtr operand = parseUnary();
        return std::make_unique<UnaryExpr>(op.loc, UnOp::Not, std::move(operand));
    }
    if (match(TokenType::Tilde)) {
        Token op = lexer_.current();
        ExprPtr operand = parseUnary();
        return std::make_unique<UnaryExpr>(op.loc, UnOp::BitNot, std::move(operand));
    }

    return parsePostfix();
}

inline ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parsePrimary();

    while (true) {
        if (match(TokenType::Dot)) {
            Token name = consume(TokenType::Identifier, "Expected property name after '.'");
            expr = std::make_unique<MemberExpr>(name.loc, std::move(expr),
                                                std::string(name.text));
        } else if (match(TokenType::LBracket)) {
            Token bracket = lexer_.current();
            ExprPtr index = parseExpression();
            consume(TokenType::RBracket, "Expected ']' after index");
            expr = std::make_unique<IndexExpr>(bracket.loc, std::move(expr),
                                               std::move(index));
        } else if (match(TokenType::LParen)) {
            // Function call
            Token paren = lexer_.current();
            std::vector<ExprPtr> args;

            if (!check(TokenType::RParen)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::Comma));
            }

            consume(TokenType::RParen, "Expected ')' after arguments");
            expr = std::make_unique<CallExpr>(paren.loc, std::move(expr),
                                              std::move(args));
        } else if (match(TokenType::Has)) {
            // Has expression: e has Component (or e has package.Component)
            Token hasTok = lexer_.current();
            Token compName = consume(TokenType::Identifier, "Expected component name after 'has'");
            std::string fullCompName = std::string(compName.text);
            if (match(TokenType::Dot)) {
                Token member = consume(TokenType::Identifier, "Expected component name after '.'");
                fullCompName = fullCompName + "." + std::string(member.text);
            }
            expr = std::make_unique<HasExpr>(hasTok.loc, std::move(expr),
                                             fullCompName, false, false);
        } else if (match(TokenType::Not)) {
            // Check for !has
            if (match(TokenType::Has)) {
                Token hasTok = lexer_.current();
                Token compName = consume(TokenType::Identifier, "Expected component name after '!has'");
                std::string fullCompName = std::string(compName.text);
                if (match(TokenType::Dot)) {
                    Token member = consume(TokenType::Identifier, "Expected component name after '.'");
                    fullCompName = fullCompName + "." + std::string(member.text);
                }
                expr = std::make_unique<HasExpr>(hasTok.loc, std::move(expr),
                                                 fullCompName, false, true);
            } else {
                // It's just a ! operator, not !has
                // We need to handle this differently - put the Not back and break
                // Actually, this shouldn't happen in postfix context
                // Just report an error
                throw ParseError(current().loc, "Expected 'has' after '!'");
            }
        } else {
            break;
        }
    }

    return expr;
}

inline ExprPtr Parser::parsePrimary() {
    Location loc = current().loc;

    // Literals
    if (check(TokenType::IntegerLiteral)) {
        Token tok = advance();
        int64_t value = std::stoll(std::string(tok.text));
        return std::make_unique<IntegerExpr>(loc, value);
    }
    if (check(TokenType::FloatLiteral)) {
        Token tok = advance();
        double value = std::stod(std::string(tok.text));
        return std::make_unique<FloatExpr>(loc, value);
    }
    if (check(TokenType::StringLiteral)) {
        Token tok = advance();
        std::string value = std::string(tok.text);
        // Remove quotes
        value = value.substr(1, value.size() - 2);
        return std::make_unique<StringExpr>(loc, value);
    }
    if (check(TokenType::FString)) {
        return parseFString();
    }
    if (match(TokenType::True)) {
        return std::make_unique<BoolExpr>(loc, true);
    }
    if (match(TokenType::False)) {
        return std::make_unique<BoolExpr>(loc, false);
    }

    // spawn expression
    if (peek() == TokenType::Spawn) {
        return parseSpawnExpr();
    }

    // Array literal expression: [1, 2, 3]
    if (match(TokenType::LBracket)) {
        std::vector<ExprPtr> elements;
        
        // Empty array: []
        if (check(TokenType::RBracket)) {
            advance();
            return std::make_unique<ArrayLiteralExpr>(loc, std::move(elements));
        }
        
        // Parse first element
        elements.push_back(parseExpression());
        
        // Parse remaining elements
        while (match(TokenType::Comma)) {
            elements.push_back(parseExpression());
        }
        
        consume(TokenType::RBracket, "Expected ']' after array elements");
        return std::make_unique<ArrayLiteralExpr>(loc, std::move(elements));
    }

    // Parenthesized expression
    if (match(TokenType::LParen)) {
        ExprPtr expr = parseExpression();
        consume(TokenType::RParen, "Expected ')' after expression");
        return expr;
    }

    // Identifier
    if (check(TokenType::Identifier)) {
        std::string name = std::string(current().text);
        advance();
        return std::make_unique<IdentifierExpr>(loc, name);
    }

    throw ParseError(loc, "Unexpected token: " + std::string(current().text));
}

inline ExprPtr Parser::parseSpawnExpr() {
    Location loc = current().loc;
    consume(TokenType::Spawn, "Expected 'spawn'");

    // Optional template name
    std::string templateName;
    if (check(TokenType::Identifier)) {
        Token name = consume(TokenType::Identifier, "Expected template name after 'spawn'");
        templateName = std::string(name.text);
    }

    return std::make_unique<SpawnExpr>(loc, templateName);
}

inline ExprPtr Parser::parseFString() {
    Location loc = current().loc;
    Token tok = advance(); // consume FString token
    
    std::string content = std::string(tok.text);
    // Remove f" prefix and " suffix
    // Format: f"content"
    if (content.size() >= 3 && content[0] == 'f' && content[1] == '"' && content.back() == '"') {
        content = content.substr(2, content.size() - 3);
    }
    
    std::vector<FStringExpr::Part> parts;
    size_t pos = 0;
    
    while (pos < content.size()) {
        // Find next {
        size_t bracePos = content.find('{', pos);
        
        if (bracePos == std::string::npos) {
            // No more braces, rest is text
            if (pos < content.size()) {
                std::string text = content.substr(pos);
                text = unescapeBraces(text);
                if (!text.empty()) {
                    parts.push_back({false, text, nullptr});
                }
            }
            break;
        }
        
        // Check if it's escaped {{
        if (bracePos + 1 < content.size() && content[bracePos + 1] == '{') {
            // Add text including one {
            std::string text = content.substr(pos, bracePos - pos + 1);
            text = unescapeBraces(text);
            if (!text.empty()) {
                parts.push_back({false, text, nullptr});
            }
            pos = bracePos + 2; // Skip {{
            continue;
        }
        
        // Add text before brace
        if (bracePos > pos) {
            std::string text = content.substr(pos, bracePos - pos);
            text = unescapeBraces(text);
            if (!text.empty()) {
                parts.push_back({false, text, nullptr});
            }
        }
        
        // Find closing } (handle nested braces)
        size_t closeBrace = bracePos + 1;
        int braceDepth = 1;
        while (closeBrace < content.size() && braceDepth > 0) {
            if (content[closeBrace] == '{') braceDepth++;
            else if (content[closeBrace] == '}') braceDepth--;
            closeBrace++;
        }
        
        if (braceDepth != 0) {
            throw ParseError(loc, "Unclosed expression in template string");
        }
        
        // Extract expression text (excluding outer braces)
        std::string exprText = content.substr(bracePos + 1, closeBrace - bracePos - 2);
        
        // Trim whitespace
        size_t start = exprText.find_first_not_of(" \t\n\r");
        size_t end = exprText.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            exprText = exprText.substr(start, end - start + 1);
        }
        
        // Parse the expression using a temporary parser
        if (!exprText.empty()) {
            Parser exprParser(exprText, packageName_);
            auto expr = exprParser.parseExpression();
            parts.push_back({true, "", std::move(expr)});
        }
        
        pos = closeBrace;
    }
    
    return std::make_unique<FStringExpr>(loc, std::move(parts));
}

// Unescape braces helper - defined outside class
inline std::string Parser::unescapeBraces(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '{' && i + 1 < s.size() && s[i + 1] == '{') {
            result += '{';
            i++;
        } else if (s[i] == '}' && i + 1 < s.size() && s[i + 1] == '}') {
            result += '}';
            i++;
        } else {
            result += s[i];
        }
    }
    return result;
}

// Statement parsing
inline StmtPtr Parser::parseStatement() {
    switch (peek()) {
        case TokenType::LBrace:
            return parseBlock();
        case TokenType::Let:
        case TokenType::Var:
            return parseVarDecl();
        case TokenType::If:
            return parseIfStmt();
        case TokenType::While:
            return parseWhileStmt();
        case TokenType::Return:
            return parseReturnStmt();
        case TokenType::Break:
            return parseBreakStmt();
        case TokenType::Continue:
            return parseContinueStmt();
        case TokenType::Exit:
            return parseExitStmt();
        case TokenType::Give:
            return parseGiveStmt();
        case TokenType::Tag:
            return parseTagStmt();
        case TokenType::Untag:
            return parseUntagStmt();
        case TokenType::Destroy:
            return parseDestroyStmt();
        case TokenType::Query:
            return parseQueryStmt();
        case TokenType::Lock:
            return parseLockStmt();
        case TokenType::Unlock:
            return parseUnlockStmt();
        default:
            return parseExprStmt();
    }
}

inline StmtPtr Parser::parseBlock() {
    Location loc = current().loc;
    consume(TokenType::LBrace, "Expected '{'");

    std::vector<StmtPtr> statements;
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        statements.push_back(parseStatement());
    }

    consume(TokenType::RBrace, "Expected '}' after block");

    return std::make_unique<BlockStmt>(loc, std::move(statements));
}

inline StmtPtr Parser::parseVarDecl() {
    Location loc = current().loc;
    bool isMutable = match(TokenType::Var);
    if (!isMutable) {
        consume(TokenType::Let, "Expected 'let' or 'var'");
    }

    Token name = consume(TokenType::Identifier, "Expected variable name");

    TypePtr type = nullptr;
    if (match(TokenType::Colon)) {
        type = parseType();
    }

    ExprPtr init = nullptr;
    if (match(TokenType::Assign)) {
        init = parseExpression();
    }

    consume(TokenType::Semicolon, "Expected ';' after variable declaration");

    return std::make_unique<VarDeclStmt>(loc, std::string(name.text),
                                         std::move(type), isMutable,
                                         std::move(init));
}

inline StmtPtr Parser::parseIfStmt() {
    Location loc = current().loc;
    consume(TokenType::If, "Expected 'if'");

    ExprPtr condition = parseExpression();
    StmtPtr thenBranch = parseStatement();

    StmtPtr elseBranch = nullptr;
    if (match(TokenType::Else)) {
        elseBranch = parseStatement();
    }

    return std::make_unique<IfStmt>(loc, std::move(condition),
                                    std::move(thenBranch), std::move(elseBranch));
}

inline StmtPtr Parser::parseWhileStmt() {
    Location loc = current().loc;
    consume(TokenType::While, "Expected 'while'");

    ExprPtr condition = parseExpression();
    StmtPtr body = parseStatement();

    return std::make_unique<WhileStmt>(loc, std::move(condition),
                                       std::move(body));
}



inline StmtPtr Parser::parseReturnStmt() {
    Location loc = current().loc;
    consume(TokenType::Return, "Expected 'return'");

    ExprPtr value = nullptr;
    if (!check(TokenType::Semicolon)) {
        value = parseExpression();
    }

    consume(TokenType::Semicolon, "Expected ';' after return");

    return std::make_unique<ReturnStmt>(loc, std::move(value));
}

inline StmtPtr Parser::parseBreakStmt() {
    Location loc = current().loc;
    consume(TokenType::Break, "Expected 'break'");
    consume(TokenType::Semicolon, "Expected ';' after break");
    return std::make_unique<BreakStmt>(loc);
}

inline StmtPtr Parser::parseContinueStmt() {
    Location loc = current().loc;
    consume(TokenType::Continue, "Expected 'continue'");
    consume(TokenType::Semicolon, "Expected ';' after continue");
    return std::make_unique<ContinueStmt>(loc);
}

inline StmtPtr Parser::parseExitStmt() {
    Location loc = current().loc;
    consume(TokenType::Exit, "Expected 'exit'");
    consume(TokenType::Semicolon, "Expected ';' after exit");
    return std::make_unique<ExitStmt>(loc);
}

inline StmtPtr Parser::parseGiveStmt() {
    Location loc = current().loc;
    consume(TokenType::Give, "Expected 'give'");

    ExprPtr entity = parseExpression();
    
    // Parse component name with optional package prefix (e.g., "engine.PlayerData")
    Token comp = consume(TokenType::Identifier, "Expected component name");
    std::string compName = std::string(comp.text);
    if (match(TokenType::Dot)) {
        Token member = consume(TokenType::Identifier, "Expected component name after '.'");
        compName = compName + "." + std::string(member.text);
    }

    std::vector<std::pair<std::string, ExprPtr>> inits;
    if (match(TokenType::Colon)) {
        inits = parseFieldInits();
    }

    consume(TokenType::Semicolon, "Expected ';' after give statement");

    return std::make_unique<GiveStmt>(loc, std::move(entity),
                                      compName, std::move(inits));
}

inline StmtPtr Parser::parseTagStmt() {
    Location loc = current().loc;
    consume(TokenType::Tag, "Expected 'tag'");

    ExprPtr entity = parseExpression();
    consume(TokenType::As, "Expected 'as' after entity");
    
    // Parse trait name with optional package prefix (e.g., "engine.TraitName")
    Token trait = consume(TokenType::Identifier, "Expected trait name");
    std::string traitName = std::string(trait.text);
    if (match(TokenType::Dot)) {
        Token member = consume(TokenType::Identifier, "Expected trait name after '.'");
        traitName = traitName + "." + std::string(member.text);
    }

    consume(TokenType::Semicolon, "Expected ';' after tag statement");

    return std::make_unique<TagStmt>(loc, std::move(entity), traitName);
}

inline StmtPtr Parser::parseUntagStmt() {
    Location loc = current().loc;
    consume(TokenType::Untag, "Expected 'untag'");

    ExprPtr entity = parseExpression();
    consume(TokenType::As, "Expected 'as' after entity");
    
    // Parse trait name with optional package prefix (e.g., "engine.TraitName")
    Token trait = consume(TokenType::Identifier, "Expected trait name");
    std::string traitName = std::string(trait.text);
    if (match(TokenType::Dot)) {
        Token member = consume(TokenType::Identifier, "Expected trait name after '.'");
        traitName = traitName + "." + std::string(member.text);
    }

    consume(TokenType::Semicolon, "Expected ';' after untag statement");

    return std::make_unique<UntagStmt>(loc, std::move(entity), traitName);
}

inline StmtPtr Parser::parseDestroyStmt() {
    Location loc = current().loc;
    consume(TokenType::Destroy, "Expected 'destroy'");

    ExprPtr entity = nullptr;
    if (!check(TokenType::Semicolon)) {
        entity = parseExpression();
    }

    consume(TokenType::Semicolon, "Expected ';' after destroy");

    return std::make_unique<DestroyStmt>(loc, std::move(entity));
}

inline StmtPtr Parser::parseQueryStmt() {
    Location loc = current().loc;
    consume(TokenType::Query, "Expected 'query'");

    std::vector<std::string> traits;
    do {
        Token trait = consume(TokenType::Identifier, "Expected trait name");
        traits.push_back(std::string(trait.text));
    } while (match(TokenType::Comma));

    std::vector<std::string> components;
    if (match(TokenType::With)) {
        do {
            Token comp = consume(TokenType::Identifier, "Expected component name");
            components.push_back(std::string(comp.text));
        } while (match(TokenType::Comma));
    }

    consume(TokenType::As, "Expected 'as' after query");
    Token var = consume(TokenType::Identifier, "Expected variable name");

    auto body = dynamic_cast<BlockStmt*>(parseBlock().release());

    return std::make_unique<QueryStmt>(loc, std::move(traits),
                                       std::move(components),
                                       std::string(var.text),
                                       std::unique_ptr<BlockStmt>(body));
}

inline StmtPtr Parser::parseLockStmt() {
    Location loc = current().loc;
    consume(TokenType::Lock, "Expected 'lock'");

    // Parse system path (e.g., engine.gravitySystem or gravitySystem)
    std::vector<std::string> path;
    Token first = consume(TokenType::Identifier, "Expected system name");
    path.push_back(std::string(first.text));

    while (match(TokenType::Dot)) {
        Token part = consume(TokenType::Identifier, "Expected system name after '.'");
        path.push_back(std::string(part.text));
    }

    consume(TokenType::Semicolon, "Expected ';' after lock");
    return std::make_unique<LockStmt>(loc, std::move(path));
}

inline StmtPtr Parser::parseUnlockStmt() {
    Location loc = current().loc;
    consume(TokenType::Unlock, "Expected 'unlock'");

    // Parse system path (e.g., engine.gravitySystem or gravitySystem)
    std::vector<std::string> path;
    Token first = consume(TokenType::Identifier, "Expected system name");
    path.push_back(std::string(first.text));

    while (match(TokenType::Dot)) {
        Token part = consume(TokenType::Identifier, "Expected system name after '.'");
        path.push_back(std::string(part.text));
    }

    consume(TokenType::Semicolon, "Expected ';' after unlock");
    return std::make_unique<UnlockStmt>(loc, std::move(path));
}

inline StmtPtr Parser::parseExprStmt() {
    Location loc = current().loc;
    ExprPtr expr = parseExpression();
    consume(TokenType::Semicolon, "Expected ';' after expression");
    return std::make_unique<ExprStmt>(loc, std::move(expr));
}

// Declaration parsing
inline std::unique_ptr<Package> Parser::parse() {
    Location loc = current().loc;
    auto pkg = std::make_unique<Package>(loc, packageName_);
    pkg->isEntryPoint = isEntryPoint_;

    while (!isAtEnd()) {
        try {
            // Try to parse statement first (for global code in any package)
            // Check if this looks like a declaration
            bool isDecl = false;
            switch (peek()) {
                case TokenType::Use:
                case TokenType::Extern:
                case TokenType::Handle:
                case TokenType::Data:
                case TokenType::Trait:
                case TokenType::Template:
                case TokenType::System:
                case TokenType::Depend:
                case TokenType::Export:
                case TokenType::Fn:
                case TokenType::For:
                case TokenType::Hash:  // For attributes like #batch
                    isDecl = true;
                    break;
                default:
                    isDecl = false;
            }
            
            if (!isDecl) {
                // Try to parse as statement (global code)
                auto stmt = parseStatement();
                if (stmt) {
                    pkg->globalStatements.push_back(std::move(stmt));
                    continue;
                }
            }
            
            auto decl = parseDeclaration();
            if (decl) {
                if (auto* use = dynamic_cast<UseDecl*>(decl.get())) {
                    pkg->uses.push_back(std::unique_ptr<UseDecl>(use));
                    decl.release();
                } else if (auto* ext = dynamic_cast<ExternFnDecl*>(decl.get())) {
                    pkg->externFunctions.push_back(std::unique_ptr<ExternFnDecl>(ext));
                    decl.release();
                } else if (auto* extType = dynamic_cast<ExternTypeDecl*>(decl.get())) {
                    pkg->externTypes.push_back(std::unique_ptr<ExternTypeDecl>(extType));
                    decl.release();
                } else if (auto* handle = dynamic_cast<HandleDecl*>(decl.get())) {
                    pkg->handles.push_back(std::unique_ptr<HandleDecl>(handle));
                    decl.release();
                } else if (auto* exp = dynamic_cast<ExportDecl*>(decl.get())) {
                    pkg->exports.push_back(std::unique_ptr<ExportDecl>(exp));
                    decl.release();
                } else if (auto* d = dynamic_cast<DataDecl*>(decl.get())) {
                    pkg->datas.push_back(std::unique_ptr<DataDecl>(d));
                    decl.release();
                } else if (auto* t = dynamic_cast<TraitDecl*>(decl.get())) {
                    pkg->traits.push_back(std::unique_ptr<TraitDecl>(t));
                    decl.release();
                } else if (auto* tmpl = dynamic_cast<TemplateDecl*>(decl.get())) {
                    pkg->templates.push_back(std::unique_ptr<TemplateDecl>(tmpl));
                    decl.release();
                } else if (auto* s = dynamic_cast<SystemDecl*>(decl.get())) {
                    pkg->systems.push_back(std::unique_ptr<SystemDecl>(s));
                    decl.release();
                } else if (auto* d = dynamic_cast<DependencyDecl*>(decl.get())) {
                    pkg->dependencies.push_back(std::unique_ptr<DependencyDecl>(d));
                    decl.release();
                } else if (auto* f = dynamic_cast<FnDecl*>(decl.get())) {
                    if (f->isExport) {
                        // Also add to exports for convenience
                    }
                    pkg->functions.push_back(std::unique_ptr<FnDecl>(f));
                    decl.release();
                }
            }
        } catch (const ParseError& e) {
            // Error recovery with color
            ConsoleColor::printErrorPrefix();
            std::cerr << " " << e.what() << "\n";
            synchronize();
        }
    }

    return pkg;
}

inline std::unique_ptr<Decl> Parser::parseDeclaration() {
    // Check for #batch attribute before system declarations
    bool isBatch = false;
    if (peek() == TokenType::Hash) {
        consume(TokenType::Hash, "Expected '#'");
        Token attr = consume(TokenType::Identifier, "Expected attribute name after '#'");
        if (attr.text == "batch") {
            isBatch = true;
        } else {
            throw ParseError(attr.loc, "Unknown attribute: " + std::string(attr.text));
        }
    }
    
    switch (peek()) {
        case TokenType::Use:
            return parseUseDecl();
        case TokenType::Extern: {
            // Check if it's extern fn or extern type
            advance(); // consume 'extern'
            if (check(TokenType::Fn)) {
                // Put back and parse as extern fn
                // Actually we already consumed extern, so we need to handle this differently
                // Let's just check current token
                return parseExternFnDecl();
            } else if (check(TokenType::Type)) {
                return parseExternTypeDecl();
            } else {
                throw ParseError(current().loc, "Expected 'fn' or 'type' after 'extern'");
            }
        }
        case TokenType::Handle:
            return parseHandleDecl();
        case TokenType::Data:
            return parseDataDecl();
        case TokenType::Trait:
            return parseTraitDecl();
        case TokenType::Template:
            return parseTemplateDecl();
        case TokenType::System:
            return parseSystemDecl(isBatch);
        case TokenType::Depend:
            return parseDependencyDecl();
        case TokenType::Export: {
            // Look ahead to check what follows 'export'
            // We need to consume 'export' first, then check the next token
            advance(); // consume 'export'
            if (check(TokenType::Fn)) {
                // It's 'export fn', handle it in parseFnDecl
                return parseFnDecl(true);
            } else if (check(TokenType::Data)) {
                // It's 'export data', handle it in parseDataDecl
                return parseDataDecl(true);
            } else if (check(TokenType::Trait)) {
                // It's 'export trait', handle it in parseTraitDecl
                return parseTraitDecl(true);
            } else if (check(TokenType::Template)) {
                // It's 'export template', handle it in parseTemplateDecl
                return parseTemplateDecl(true);
            } else if (check(TokenType::Handle)) {
                // It's 'export handle', handle it in parseHandleDecl
                return parseHandleDecl(true);
            } else {
                // It's 'export Identifier' (for extern type/function), handle it in parseExportDecl
                return parseExportDeclAfterConsume();
            }
        }
        case TokenType::Fn:
            return parseFnDecl();
        case TokenType::For:
            return parseForTraitSystem(isBatch);
        default:
            throw ParseError(current().loc, "Expected declaration");
    }
}

inline std::unique_ptr<UseDecl> Parser::parseUseDecl() {
    Location loc = current().loc;
    consume(TokenType::Use, "Expected 'use'");

    // Parse package name (single identifier, no sub-modules)
    Token pkgName = consume(TokenType::Identifier, "Expected package name");
    std::vector<std::string> path;
    path.push_back(std::string(pkgName.text));

    // Check for 'as' clause
    std::string alias;
    if (match(TokenType::As)) {
        Token aliasTok = consume(TokenType::Identifier, "Expected alias after 'as'");
        alias = std::string(aliasTok.text);
    }

    consume(TokenType::Semicolon, "Expected ';' after use");
    
    // 关键：根据是否是入口文件决定 onlyUtils 标志
    // 入口文件 use = 完整加载（onlyUtils = false）
    // 非入口文件 use = 只加载 utils（onlyUtils = true）
    bool onlyUtils = !isEntryPoint_;
    
    return std::make_unique<UseDecl>(loc, std::move(path), std::move(alias), onlyUtils);
}

inline std::unique_ptr<ExportDecl> Parser::parseExportDecl() {
    Location loc = current().loc;
    consume(TokenType::Export, "Expected 'export'");
    return parseExportDeclAfterConsume();
}

inline std::unique_ptr<ExportDecl> Parser::parseExportDeclAfterConsume() {
    // 'export' already consumed by caller
    Location loc = current().loc;

    // export TypeName or export funcName
    Token nameTok = consume(TokenType::Identifier, "Expected type or function name after 'export'");
    consume(TokenType::Semicolon, "Expected ';' after export");

    // We don't know if it's a type or function at parse time
    // isType = false means it could be either, semantic analysis will determine
    return std::make_unique<ExportDecl>(loc, std::string(nameTok.text), false);
}

inline std::unique_ptr<ExternTypeDecl> Parser::parseExternTypeDecl() {
    Location loc = current().loc;
    // 'extern' already consumed by caller
    consume(TokenType::Type, "Expected 'type' after 'extern'");

    Token name = consume(TokenType::Identifier, "Expected type name");

    consume(TokenType::LBracket, "Expected '[' after type name");
    Token sizeToken = consume(TokenType::IntegerLiteral, "Expected size in bytes");
    consume(TokenType::RBracket, "Expected ']' after size");

    consume(TokenType::Semicolon, "Expected ';' after extern type declaration");

    uint32_t size = std::stoul(std::string(sizeToken.text));
    return std::make_unique<ExternTypeDecl>(loc, std::string(name.text), size);
}

inline std::unique_ptr<HandleDecl> Parser::parseHandleDecl(bool exported) {
    Location loc = current().loc;
    consume(TokenType::Handle, "Expected 'handle'");

    Token name = consume(TokenType::Identifier, "Expected handle type name");

    consume(TokenType::With, "Expected 'with' after handle type name");

    Token dtor = consume(TokenType::Identifier, "Expected destructor function name");

    consume(TokenType::Semicolon, "Expected ';' after handle declaration");

    return std::make_unique<HandleDecl>(loc, std::string(name.text), std::string(dtor.text), exported);
}

inline std::unique_ptr<ExternFnDecl> Parser::parseExternFnDecl() {
    Location loc = current().loc;
    // 'extern' already consumed by caller
    consume(TokenType::Fn, "Expected 'fn' after 'extern'");

    Token name = consume(TokenType::Identifier, "Expected function name");

    consume(TokenType::LParen, "Expected '(' after function name");

    std::vector<Param> params;
    bool isVariadic = false;

    if (!check(TokenType::RParen)) {
        do {
            // Check for variadic ...
            if (match(TokenType::Dot)) {
                consume(TokenType::Dot, "Expected '.'");
                consume(TokenType::Dot, "Expected '.'");
                isVariadic = true;
                break;
            }

            Param param = parseParam();
            params.push_back(std::move(param));
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RParen, "Expected ')' after parameters");
    consume(TokenType::Arrow, "Expected '->' before return type");

    TypePtr returnType = parseType();
    consume(TokenType::Semicolon, "Expected ';' after extern function declaration");

    return std::make_unique<ExternFnDecl>(loc, std::string(name.text), std::move(params), std::move(returnType), isVariadic);
}

inline std::unique_ptr<DataDecl> Parser::parseDataDecl(bool exported) {
    Location loc = current().loc;
    consume(TokenType::Data, "Expected 'data'");

    Token name = consume(TokenType::Identifier, "Expected data name");
    consume(TokenType::LBrace, "Expected '{' after data name");

    std::vector<Field> fields;
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        fields.push_back(parseField());
        if (!check(TokenType::RBrace)) {
            consume(TokenType::Comma, "Expected ',' or '}' after field");
        }
    }

    consume(TokenType::RBrace, "Expected '}' after data fields");

    return std::make_unique<DataDecl>(loc, std::string(name.text),
                                      std::move(fields), exported);
}

inline std::unique_ptr<TraitDecl> Parser::parseTraitDecl(bool exported) {
    Location loc = current().loc;
    consume(TokenType::Trait, "Expected 'trait'");

    Token name = consume(TokenType::Identifier, "Expected trait name");

    std::vector<std::string> required;
    if (match(TokenType::LBracket)) {
        do {
            Token comp = consume(TokenType::Identifier, "Expected component name");
            required.push_back(std::string(comp.text));
        } while (match(TokenType::Comma));
        consume(TokenType::RBracket, "Expected ']' after trait requirements");
    }

    consume(TokenType::Semicolon, "Expected ';' after trait declaration");

    return std::make_unique<TraitDecl>(loc, std::string(name.text),
                                       std::move(required), exported);
}

inline std::unique_ptr<TemplateDecl> Parser::parseTemplateDecl(bool exported) {
    Location loc = current().loc;
    consume(TokenType::Template, "Expected 'template'");

    Token name = consume(TokenType::Identifier, "Expected template name");
    consume(TokenType::LBrace, "Expected '{' after template name");

    std::vector<TemplateComponentInit> components;
    std::vector<std::string> tags;

    while (!check(TokenType::RBrace) && !isAtEnd()) {
        // Check for "tags" identifier
        if (check(TokenType::Identifier) && current().text == "tags") {
            advance(); // consume "tags"
            consume(TokenType::Colon, "Expected ':' after 'tags'");
            consume(TokenType::LBracket, "Expected '[' after ':'");
            do {
                Token tag = consume(TokenType::Identifier, "Expected tag name");
                tags.push_back(std::string(tag.text));
            } while (match(TokenType::Comma));
            consume(TokenType::RBracket, "Expected ']' after tags");
            if (!check(TokenType::RBrace)) {
                consume(TokenType::Comma, "Expected ',' after tags");
            }
        } else {
            Token comp = consume(TokenType::Identifier, "Expected component name");
            TemplateComponentInit init;
            init.componentName = std::string(comp.text);

            if (match(TokenType::Colon)) {
                init.fieldValues = parseFieldInits();
            }

            components.push_back(std::move(init));

            if (!check(TokenType::RBrace)) {
                consume(TokenType::Comma, "Expected ',' or '}' after component");
            }
        }
    }

    consume(TokenType::RBrace, "Expected '}' after template body");

    return std::make_unique<TemplateDecl>(loc, std::string(name.text),
                                          std::move(components),
                                          std::move(tags), exported);
}

inline std::unique_ptr<SystemDecl> Parser::parseSystemDecl(bool isBatch) {
    Location loc = current().loc;
    consume(TokenType::System, "Expected 'system'");

    Token name = consume(TokenType::Identifier, "Expected system name");
    consume(TokenType::LParen, "Expected '(' after system name");

    std::vector<Param> params;
    if (!check(TokenType::RParen)) {
        do {
            params.push_back(parseParam());
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RParen, "Expected ')' after parameters");

    auto body = dynamic_cast<BlockStmt*>(parseBlock().release());

    return std::make_unique<SystemDecl>(loc, std::string(name.text),
                                        std::move(params),
                                        std::unique_ptr<BlockStmt>(body),
                                        isBatch);
}

inline std::unique_ptr<SystemDecl> Parser::parseForTraitSystem(bool isBatch) {
    Location loc = current().loc;
    consume(TokenType::For, "Expected 'for'");

    // Parse trait name, optionally with package prefix (e.g., "engine.Movable")
    Token first = consume(TokenType::Identifier, "Expected trait name");
    std::string traitName = std::string(first.text);
    
    // Check for package qualifier: package.Trait
    if (match(TokenType::Dot)) {
        Token second = consume(TokenType::Identifier, "Expected trait name after '.'");
        traitName = std::string(first.text) + "__" + std::string(second.text);
    }
    
    std::vector<std::string> traits;
    traits.push_back(traitName);

    consume(TokenType::In, "Expected 'in' after trait");

    Token name = consume(TokenType::Identifier, "Expected system name");
    consume(TokenType::LParen, "Expected '(' after system name");

    std::vector<Param> params;
    if (!check(TokenType::RParen)) {
        do {
            params.push_back(parseParam());
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RParen, "Expected ')' after parameters");
    
    // Parse mandatory "as entityVar" clause
    consume(TokenType::As, "Expected 'as' after system parameters (entity variable name is required)");
    Token varTok = consume(TokenType::Identifier, "Expected entity variable name after 'as'");
    std::string entityVar = std::string(varTok.text);

    auto body = dynamic_cast<BlockStmt*>(parseBlock().release());

    return std::make_unique<SystemDecl>(loc, std::move(traits),
                                        std::string(name.text),
                                        std::move(params),
                                        std::unique_ptr<BlockStmt>(body),
                                        entityVar,
                                        isBatch);
}

inline std::unique_ptr<DependencyDecl> Parser::parseDependencyDecl() {
    Location loc = current().loc;
    consume(TokenType::Depend, "Expected 'depend'");

    std::vector<std::string> systems;
    
    // Parse first system name
    Token first = consume(TokenType::Identifier, "Expected system name");
    systems.push_back(std::string(first.text));
    
    // Parse chain: -> B -> C -> D
    while (match(TokenType::Arrow)) {
        Token next = consume(TokenType::Identifier, "Expected system name after '->'");
        systems.push_back(std::string(next.text));
    }

    consume(TokenType::Semicolon, "Expected ';' after dependency");

    return std::make_unique<DependencyDecl>(loc, std::move(systems));
}

inline std::unique_ptr<FnDecl> Parser::parseFnDecl(bool alreadyConsumedExport) {
    Location loc = current().loc;
    bool isExport = alreadyConsumedExport || match(TokenType::Export);

    if (!alreadyConsumedExport && isExport) {
        consume(TokenType::Fn, "Expected 'fn' after 'export'");
    } else {
        consume(TokenType::Fn, "Expected 'fn'");
    }

    Token name = consume(TokenType::Identifier, "Expected function name");
    consume(TokenType::LParen, "Expected '(' after function name");

    std::vector<Param> params;
    if (!check(TokenType::RParen)) {
        do {
            params.push_back(parseParam());
        } while (match(TokenType::Comma));
    }

    consume(TokenType::RParen, "Expected ')' after parameters");

    TypePtr returnType = Type::void_();
    if (match(TokenType::Arrow)) {
        returnType = parseType();
    }

    auto body = dynamic_cast<BlockStmt*>(parseBlock().release());

    auto fn = std::make_unique<FnDecl>(loc, std::string(name.text),
                                       std::move(params),
                                       std::move(returnType),
                                       std::unique_ptr<BlockStmt>(body));
    fn->isExport = isExport;
    return fn;
}



// Helper parsing
inline Field Parser::parseField() {
    Token name = consume(TokenType::Identifier, "Expected field name");
    consume(TokenType::Colon, "Expected ':' after field name");

    TypePtr type = parseType();

    ExprPtr defaultValue = nullptr;
    if (match(TokenType::Assign)) {
        defaultValue = parseExpression();
    }

    return Field{std::string(name.text), std::move(type), std::move(defaultValue)};
}

inline Param Parser::parseParam() {
    bool isMutable = match(TokenType::Var);

    Token name = consume(TokenType::Identifier, "Expected parameter name");
    consume(TokenType::Colon, "Expected ':' after parameter name");

    TypePtr type = parseType();

    return Param{std::string(name.text), std::move(type), isMutable};
}

inline std::vector<std::pair<std::string, ExprPtr>> Parser::parseFieldInits() {
    std::vector<std::pair<std::string, ExprPtr>> inits;

    consume(TokenType::LBrace, "Expected '{' for field initialization");

    while (!check(TokenType::RBrace) && !isAtEnd()) {
        Token field = consume(TokenType::Identifier, "Expected field name");
        consume(TokenType::Colon, "Expected ':' after field name");
        ExprPtr value = parseExpression();

        inits.emplace_back(std::string(field.text), std::move(value));

        if (!check(TokenType::RBrace)) {
            consume(TokenType::Comma, "Expected ',' or '}' after field init");
        }
    }

    consume(TokenType::RBrace, "Expected '}' after field initialization");

    return inits;
}

// Add check method
inline bool Parser::check(TokenType type) const {
    return peek() == type;
}

} // namespace paani

#endif // PAANI_PARSER_HPP

#ifndef PAANI_PARSER_RD_HPP
#define PAANI_PARSER_RD_HPP

#include "paani_lexer.hpp"
#include "paani_ast.hpp"
#include <memory>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

namespace paani {

// Forward declarations
class Parser;

// ============ Semantic Checker ============

class SemanticError : public std::runtime_error {
public:
    SemanticError(const std::string& msg, Position pos) 
        : std::runtime_error(formatMsg(msg, pos)), position(pos) {}
    
    Position getPosition() const { return position; }
    
private:
    Position position;
    
    static std::string formatMsg(const std::string& msg, Position pos) {
        return "Semantic error at line " + std::to_string(pos.line) + 
               ", column " + std::to_string(pos.column) + ": " + msg;
    }
};

class SemanticChecker {
public:
    void check(Package& pkg);
    
private:
    // 全局定义
    std::unordered_set<std::string> dataNames;
    std::unordered_set<std::string> traitNames;
    std::unordered_set<std::string> templateNames;
    std::unordered_set<std::string> systemNames;
    std::unordered_set<std::string> functionNames;
    
    // 变量作用域栈（每个作用域是变量名到可变性的映射）
    std::vector<std::unordered_map<std::string, bool>> varScopes;  // bool: is_mutable
    // 当前函数参数及其可变性
    std::unordered_map<std::string, bool> currentFunctionParams;  // bool: is_mutable
    // 当前系统参数（for Trait in system）- 默认可变
    std::unordered_set<std::string> currentSystemParams;
    // query 语句中的迭代变量 - 默认可变
    std::unordered_set<std::string> currentQueryVars;
    
    void collectDefinitions(Package& pkg);
    
    // 作用域管理
    void pushScope();
    void popScope();
    void addVariable(const std::string& name, bool isMutable, Position pos);
    bool isVariableDefined(const std::string& name);
    bool isVariableMutable(const std::string& name);  // 检查变量是否可变
    
    // 检查函数
    void checkTrait(TraitDecl& trait, Package& pkg);
    void checkTemplate(TemplateDecl& tmpl, Package& pkg);
    void checkSystem(SystemDecl& sys, Package& pkg);
    void checkFunction(FnDecl& fn, Package& pkg);
    void checkDependency(DependencyDecl& dep, Package& pkg);
    void checkBlock(Block& block, Package& pkg);
    void checkStatement(Stmt& stmt, Package& pkg);
    void checkGiveStmt(GiveStmt& stmt, Package& pkg);
    void checkTagStmt(TagStmt& stmt, Package& pkg);
    void checkUntagStmt(UntagStmt& stmt, Package& pkg);
    void checkDestroyStmt(DestroyStmt& stmt, Package& pkg);
    void checkQueryStmt(QueryStmt& stmt, Package& pkg);
    void checkForStmt(ForStmt& stmt, Package& pkg);
    void checkForInStmt(ForInStmt& stmt, Package& pkg);
    void checkVarDecl(VarDecl& stmt, Package& pkg);
    void checkExpression(Expr& expr, Package& pkg);
    void checkIdentifier(Identifier& id, Package& pkg);
};

class Parser {
public:
    explicit Parser(std::string_view source, const std::string& packageName = "main") 
        : lexer_(source), packageName_(packageName) {}
    
    std::unique_ptr<Package> parse() {
        auto pkg = std::make_unique<Package>(makePos(), packageName_);

        while (!lexer_.isAtEnd()) {
            auto decl = parseDeclaration();
            if (decl) {
                if (auto* d = dynamic_cast<DataDecl*>(decl.get())) {
                    pkg->datas.push_back(std::unique_ptr<DataDecl>(static_cast<DataDecl*>(decl.release())));
                } else if (auto* t = dynamic_cast<TraitDecl*>(decl.get())) {
                    pkg->traits.push_back(std::unique_ptr<TraitDecl>(static_cast<TraitDecl*>(decl.release())));
                } else if (auto* tmpl = dynamic_cast<TemplateDecl*>(decl.get())) {
                    pkg->templates.push_back(std::unique_ptr<TemplateDecl>(static_cast<TemplateDecl*>(decl.release())));
                } else if (auto* s = dynamic_cast<SystemDecl*>(decl.get())) {
                    pkg->systems.push_back(std::unique_ptr<SystemDecl>(static_cast<SystemDecl*>(decl.release())));
                } else if (auto* d = dynamic_cast<DependencyDecl*>(decl.get())) {
                    pkg->dependencies.push_back(std::unique_ptr<DependencyDecl>(static_cast<DependencyDecl*>(decl.release())));
                } else if (auto* f = dynamic_cast<FnDecl*>(decl.get())) {
                    if (f->is_export) {
                        pkg->exports.push_back(std::unique_ptr<FnDecl>(static_cast<FnDecl*>(decl.release())));
                    } else {
                        pkg->functions.push_back(std::unique_ptr<FnDecl>(static_cast<FnDecl*>(decl.release())));
                    }
                }
            }
        }

        // Run semantic checks
        SemanticChecker checker;
        checker.check(*pkg);

        return pkg;
    }

private:
    Lexer lexer_;
    std::string packageName_;
    
    Position makePos() {
        return Position{static_cast<int>(lexer_.line()), static_cast<int>(lexer_.column())};
    }
    
    void error(const std::string& msg) {
        auto tok = lexer_.peek();
        throw std::runtime_error("Parse error at line " + std::to_string(tok.line) + 
                                ", column " + std::to_string(tok.column) + ": " + msg);
    }
    
    void expect(TokenType type) {
        if (lexer_.peek().type != type) {
            error("Expected " + tokenTypeName(type) + ", got " + lexer_.peek().text);
        }
        lexer_.next();
    }
    
    std::string tokenTypeName(TokenType type) {
        switch (type) {
            case TokenType::END: return "end of file";
            case TokenType::DATA: return "'data'";
            case TokenType::TRAIT: return "'trait'";
            case TokenType::TEMPLATE: return "'template'";
            case TokenType::SYSTEM: return "'system'";
            case TokenType::IDENTIFIER: return "identifier";
            case TokenType::LBRACE: return "'{'";
            case TokenType::RBRACE: return "'}'";
            case TokenType::SEMICOLON: return "';'";
            default: return "token";
        }
    }

    // data_field := identifier ':' type ('=' expr)?
    Field parseDataField() {
        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);

        expect(TokenType::COLON);

        auto fieldType = parseType();

        // 可选默认值
        std::unique_ptr<Expr> defaultValue = nullptr;
        if (lexer_.match(TokenType::ASSIGN)) {
            defaultValue = parseExpression();
        }

        return Field{nameTok.text, fieldType, std::move(defaultValue)};
    }
    
    // declaration := data_decl | trait_decl | template_decl | system_decl | dependency_decl | export_decl | fn_decl
    std::unique_ptr<Decl> parseDeclaration() {
        auto tok = lexer_.peek();

        switch (tok.type) {
            case TokenType::DATA:
                return parseDataDecl();
            case TokenType::TRAIT:
                return parseTraitDecl();
            case TokenType::TEMPLATE:
                return parseTemplateDecl();
            case TokenType::SYSTEM:
                return parseSystemDecl();
            case TokenType::FOR:
                return parseForTraitSystemDecl();
            case TokenType::DEPENDENCY:
            case TokenType::DEPEND:
                return parseDependencyDecl();
            case TokenType::EXPORT:
                return parseExportDecl();
            case TokenType::FN:
                return parseFnDecl();
            case TokenType::EXTERN:
                return parseExternDecl();
            case TokenType::STRUCT:
                return parseStructDecl();
            case TokenType::IMPL:
                return parseImplDecl();
            default:
                error("Unexpected token: " + tok.text);
                return nullptr;
        }
    }
    
    // data_decl := 'data' identifier '{' field_list '}'
    std::unique_ptr<Decl> parseDataDecl() {
        auto pos = makePos();
        expect(TokenType::DATA);

        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string name = nameTok.text;

        expect(TokenType::LBRACE);

        std::vector<Field> fields;
        while (lexer_.peek().type != TokenType::RBRACE && !lexer_.isAtEnd()) {
            fields.push_back(parseDataField());
            if (lexer_.peek().type == TokenType::COMMA) {
                lexer_.next();
            }
        }

        expect(TokenType::RBRACE);

        return std::make_unique<DataDecl>(pos, name, std::move(fields));
    }
    
    // trait_decl := 'trait' identifier ('[' identifier_list ']')? ';'
    // trait Movable;              // 纯标签
    // trait Movable [Pos, Vel];   // 有依赖
    std::unique_ptr<Decl> parseTraitDecl() {
        auto pos = makePos();
        expect(TokenType::TRAIT);

        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string name = nameTok.text;

        std::vector<std::string> requiredData;

        // 可选的依赖列表
        if (lexer_.match(TokenType::LBRACKET)) {
            // 解析 [Comp1, Comp2, ...]
            while (lexer_.peek().type != TokenType::RBRACKET && !lexer_.isAtEnd()) {
                auto compTok = lexer_.peek();
                expect(TokenType::IDENTIFIER);
                requiredData.push_back(compTok.text);

                if (lexer_.peek().type == TokenType::COMMA) {
                    lexer_.next();
                }
            }
            expect(TokenType::RBRACKET);
        }

        expect(TokenType::SEMICOLON);

        return std::make_unique<TraitDecl>(pos, name, std::move(requiredData));
    }

    // template_decl := 'template' identifier '{' component_init_list (',' 'tags' ':' '[' identifier_list ']')? '}'
    std::unique_ptr<Decl> parseTemplateDecl() {
        auto pos = makePos();
        expect(TokenType::TEMPLATE);

        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string name = nameTok.text;

        expect(TokenType::LBRACE);

        std::vector<TemplateComponentInit> components;
        std::vector<std::string> tags;

        while (lexer_.peek().type != TokenType::RBRACE && !lexer_.isAtEnd()) {
            // 检查是否是 tags 字段
            if (lexer_.peek().type == TokenType::IDENTIFIER && lexer_.peek().text == "tags") {
                lexer_.next();
                expect(TokenType::COLON);
                expect(TokenType::LBRACKET);

                while (lexer_.peek().type != TokenType::RBRACKET && !lexer_.isAtEnd()) {
                    auto tagTok = lexer_.peek();
                    expect(TokenType::IDENTIFIER);
                    tags.push_back(tagTok.text);

                    if (lexer_.peek().type == TokenType::COMMA) {
                        lexer_.next();
                    }
                }
                expect(TokenType::RBRACKET);
            } else {
                // 解析组件初始化
                components.push_back(parseTemplateComponentInit());
            }

            if (lexer_.peek().type == TokenType::COMMA) {
                lexer_.next();
            }
        }

        expect(TokenType::RBRACE);

        return std::make_unique<TemplateDecl>(pos, name, std::move(components), std::move(tags));
    }

    // component_init := identifier (':' '{' field_init_list '}')?
    TemplateComponentInit parseTemplateComponentInit() {
        auto compTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string compName = compTok.text;

        std::vector<TemplateFieldInit> fields;

        if (lexer_.match(TokenType::COLON)) {
            expect(TokenType::LBRACE);

            while (lexer_.peek().type != TokenType::RBRACE && !lexer_.isAtEnd()) {
                auto fieldTok = lexer_.peek();
                expect(TokenType::IDENTIFIER);
                expect(TokenType::COLON);
                auto value = parseExpression();

                fields.push_back(TemplateFieldInit{fieldTok.text, std::move(value)});

                if (lexer_.peek().type == TokenType::COMMA) {
                    lexer_.next();
                }
            }

            expect(TokenType::RBRACE);
        }

        return TemplateComponentInit{compName, std::move(fields)};
    }

    // for_trait_system_decl := 'for' identifier (',' identifier)* 'in' identifier '(' param_list ')' block
    // for Movable in move(dt: f32) { ... }
    std::unique_ptr<Decl> parseForTraitSystemDecl() {
        auto pos = makePos();
        expect(TokenType::FOR);

        // 解析 trait 列表
        std::vector<std::string> forTraits;

        auto traitTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        forTraits.push_back(traitTok.text);

        // 多 trait: for Movable, Living in ...
        while (lexer_.match(TokenType::COMMA)) {
            auto nextTraitTok = lexer_.peek();
            expect(TokenType::IDENTIFIER);
            forTraits.push_back(nextTraitTok.text);
        }

        expect(TokenType::IN);

        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string systemName = nameTok.text;

        expect(TokenType::LPAREN);
        auto params = parseParamList();
        expect(TokenType::RPAREN);

        auto body = parseBlock();

        return std::make_unique<SystemDecl>(pos, forTraits, systemName, std::move(params), std::move(body));
    }
    
    // system_decl := 'system' identifier '(' param_list ')' block
    std::unique_ptr<Decl> parseSystemDecl() {
        auto pos = makePos();
        expect(TokenType::SYSTEM);
        
        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string name = nameTok.text;
        
        expect(TokenType::LPAREN);
        auto params = parseParamList();
        expect(TokenType::RPAREN);
        
        auto body = parseBlock();
        
        return std::make_unique<SystemDecl>(pos, name, std::move(params), std::move(body));
    }
    
    // dependency_decl := ('dependency' | 'depend') identifier ('before' identifier | 'after' identifier) ';'
    // Examples:
    //   depend system1 before system2;  // system1 runs before system2
    //   depend system1 after system2;   // system1 runs after system2 (reverse)
    std::unique_ptr<Decl> parseDependencyDecl() {
        auto pos = makePos();
        
        // Accept either 'dependency' or 'depend'
        if (lexer_.match(TokenType::DEPENDENCY)) {
            // legacy 'dependency' keyword
        } else if (lexer_.match(TokenType::DEPEND)) {
            // new 'depend' keyword
        } else {
            error("Expected 'depend' or 'dependency'");
        }
        
        auto firstTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string first = firstTok.text;
        
        // Check for 'before' or 'after' syntax
        std::string before, after;
        
        if (lexer_.match(TokenType::BEFORE)) {
            // syntax: depend system1 before system2; (system1 runs before system2)
            auto secondTok = lexer_.peek();
            expect(TokenType::IDENTIFIER);
            before = first;
            after = secondTok.text;
        } else if (lexer_.match(TokenType::AFTER)) {
            // syntax: depend system1 after system2; (system1 runs after system2, i.e., system2 runs before system1)
            auto secondTok = lexer_.peek();
            expect(TokenType::IDENTIFIER);
            before = secondTok.text;  // reversed
            after = first;
        } else {
            error("Expected 'before' or 'after' in dependency declaration");
        }
        
        expect(TokenType::SEMICOLON);
        
        return std::make_unique<DependencyDecl>(pos, before, after);
    }
    
    // export_decl := 'export' fn_decl
    std::unique_ptr<Decl> parseExportDecl() {
        auto pos = makePos();
        expect(TokenType::EXPORT);
        
        auto fn = parseFnDecl();
        fn->is_export = true;
        
        return fn;
    }
    
    // extern_decl := 'extern' 'data' identifier ';'
    std::unique_ptr<Decl> parseExternDecl() {
        auto pos = makePos();
        expect(TokenType::EXTERN);

        if (lexer_.match(TokenType::DATA)) {
            auto nameTok = lexer_.peek();
            expect(TokenType::IDENTIFIER);
            expect(TokenType::SEMICOLON);

            // 外部 data 声明
            return std::make_unique<DataDecl>(pos, nameTok.text, std::vector<Field>{});
        } else if (lexer_.match(TokenType::FN)) {
            return parseFnDecl();
        }

        error("Expected 'data' or 'fn' after 'extern'");
        return nullptr;
    }
    
    // fn_decl := 'fn' identifier '(' param_list ')' ('->' type)? block
    std::unique_ptr<FnDecl> parseFnDecl() {
        auto pos = makePos();
        expect(TokenType::FN);
        
        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string name = nameTok.text;
        
        expect(TokenType::LPAREN);
        auto params = parseParamList();
        expect(TokenType::RPAREN);
        
        Type returnType{TypeKind::Void, "void"};
        if (lexer_.match(TokenType::ARROW)) {
            returnType = parseType();
        }
        
        auto body = parseBlock();
        
        auto fn = std::make_unique<FnDecl>(pos, name, std::move(params), returnType, std::move(body));
        return fn;
    }
    
    // struct_decl := 'struct' identifier '{' field_list '}'
    std::unique_ptr<Decl> parseStructDecl() {
        auto pos = makePos();
        expect(TokenType::STRUCT);

        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string name = nameTok.text;

        expect(TokenType::LBRACE);

        std::vector<Field> fields;
        while (lexer_.peek().type != TokenType::RBRACE && !lexer_.isAtEnd()) {
            fields.push_back(parseDataField());
            if (lexer_.peek().type == TokenType::COMMA) {
                lexer_.next();
            }
        }

        expect(TokenType::RBRACE);

        return std::make_unique<StructDecl>(pos, name, std::move(fields));
    }
    
    // impl_decl := 'impl' identifier '{' fn_decl* '}'
    std::unique_ptr<Decl> parseImplDecl() {
        auto pos = makePos();
        expect(TokenType::IMPL);

        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string name = nameTok.text;

        expect(TokenType::LBRACE);

        std::vector<std::unique_ptr<FnDecl>> methods;
        while (lexer_.peek().type != TokenType::RBRACE && !lexer_.isAtEnd()) {
            if (lexer_.peek().type == TokenType::FN) {
                auto fn = parseFnDecl();
                methods.push_back(std::move(fn));
            } else {
                lexer_.next(); // skip
            }
        }

        expect(TokenType::RBRACE);

        return std::make_unique<ImplDecl>(pos, name, std::move(methods));
    }
    
    // field := identifier ':' type (without default value)
    Field parseField() {
        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);

        expect(TokenType::COLON);

        auto fieldType = parseType();

        return Field{nameTok.text, fieldType, nullptr};
    }
    
    // param_list := (param (',' param)*)?
    std::vector<Param> parseParamList() {
        std::vector<Param> params;
        
        if (lexer_.peek().type == TokenType::RPAREN) {
            return params;
        }
        
        params.push_back(parseParam());
        
        while (lexer_.match(TokenType::COMMA)) {
            params.push_back(parseParam());
        }
        
        return params;
    }
    
    // param := ('let' | 'var')? identifier ':' type
    Param parseParam() {
        bool isMut = false;
        if (lexer_.match(TokenType::VAR)) {
            isMut = true;
        } else if (lexer_.match(TokenType::LET)) {
            isMut = false;
        }
        
        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        
        expect(TokenType::COLON);
        
        auto paramType = parseType();
        
        return Param{nameTok.text, paramType, isMut};
    }
    
    // type := primitive_type | 'entity' | '*' type | '[' type ';' integer ']'
    Type parseType() {
        auto tok = lexer_.peek();
        
        // Pointer type
        if (lexer_.match(TokenType::MUL)) {
            auto base = parseType();
            auto basePtr = std::make_shared<Type>(base);
            return Type{TypeKind::Pointer, base.name + "*", basePtr};
        }
        
        // Array type
        if (lexer_.match(TokenType::LBRACKET)) {
            auto base = parseType();
            expect(TokenType::SEMICOLON);
            auto sizeTok = lexer_.peek();
            expect(TokenType::INTEGER_LITERAL);
            int size = std::stoi(sizeTok.text);
            expect(TokenType::RBRACKET);
            auto basePtr = std::make_shared<Type>(base);
            return Type{TypeKind::Array, base.name + "[" + std::to_string(size) + "]", basePtr, size};
        }
        
        // Primitive types
        TypeKind kind = TypeKind::Void;
        std::string name = tok.text;
        
        switch (tok.type) {
            case TokenType::VOID: kind = TypeKind::Void; lexer_.next(); break;
            case TokenType::BOOL: kind = TypeKind::Bool; lexer_.next(); break;
            case TokenType::I8: kind = TypeKind::I8; lexer_.next(); break;
            case TokenType::I16: kind = TypeKind::I16; lexer_.next(); break;
            case TokenType::I32: kind = TypeKind::I32; lexer_.next(); break;
            case TokenType::I64: kind = TypeKind::I64; lexer_.next(); break;
            case TokenType::U8: kind = TypeKind::U8; lexer_.next(); break;
            case TokenType::U16: kind = TypeKind::U16; lexer_.next(); break;
            case TokenType::U32: kind = TypeKind::U32; lexer_.next(); break;
            case TokenType::U64: kind = TypeKind::U64; lexer_.next(); break;
            case TokenType::F32: kind = TypeKind::F32; lexer_.next(); break;
            case TokenType::F64: kind = TypeKind::F64; lexer_.next(); break;
            case TokenType::STRING: kind = TypeKind::String; lexer_.next(); break;
            case TokenType::ENTITY: kind = TypeKind::Entity; lexer_.next(); break;
            case TokenType::IDENTIFIER: 
                kind = TypeKind::UserDefined; 
                lexer_.next();
                break;
            default:
                error("Expected type, got " + tok.text);
        }
        
        return Type{kind, name};
    }
    
    // block := '{' stmt* '}'
    std::unique_ptr<Block> parseBlock() {
        auto pos = makePos();
        expect(TokenType::LBRACE);
        
        std::vector<std::unique_ptr<Stmt>> stmts;
        while (lexer_.peek().type != TokenType::RBRACE && !lexer_.isAtEnd()) {
            stmts.push_back(parseStatement());
        }
        
        expect(TokenType::RBRACE);
        
        return std::make_unique<Block>(pos, std::move(stmts));
    }
    
    // stmt := expr_stmt | var_decl | if_stmt | while_stmt | for_stmt | return_stmt | block |
    //          give_stmt | tag_stmt | untag_stmt | destroy_stmt | query_stmt
    std::unique_ptr<Stmt> parseStatement() {
        auto tok = lexer_.peek();

        switch (tok.type) {
            case TokenType::LET:
            case TokenType::VAR:
                return parseVarDecl();
            case TokenType::IF:
                return parseIfStmt();
            case TokenType::WHILE:
                return parseWhileStmt();
            case TokenType::FOR:
                return parseForStmt();
            case TokenType::RETURN:
                return parseReturnStmt();
            case TokenType::BREAK:
                return parseBreakStmt();
            case TokenType::CONTINUE:
                return parseContinueStmt();
            case TokenType::LBRACE:
                return parseBlock();
            case TokenType::GIVE:
                return parseGiveStmt();
            case TokenType::TAG:
                return parseTagStmt();
            case TokenType::UNTAG:
                return parseUntagStmt();
            case TokenType::DESTROY:
                return parseDestroyStmt();
            case TokenType::QUERY:
                return parseQueryStmt();
            default:
                return parseExprStmt();
        }
    }
    
    // var_decl := ('let' | 'var') identifier (':' type)? ('=' expr)? ';'
    std::unique_ptr<Stmt> parseVarDecl() {
        auto pos = makePos();
        
        bool isMut = false;
        if (lexer_.match(TokenType::VAR)) {
            isMut = true;
        } else {
            expect(TokenType::LET);
        }
        
        auto nameTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        
        Type varType{TypeKind::Void, "void"};
        if (lexer_.match(TokenType::COLON)) {
            varType = parseType();
        }
        
        std::unique_ptr<Expr> init = nullptr;
        if (lexer_.match(TokenType::ASSIGN)) {
            init = parseExpression();
        }
        
        expect(TokenType::SEMICOLON);
        
        return std::make_unique<VarDecl>(pos, nameTok.text, varType, isMut, std::move(init));
    }
    
    // if_stmt := 'if' expr block ('else' (if_stmt | block))?
    std::unique_ptr<Stmt> parseIfStmt() {
        auto pos = makePos();
        expect(TokenType::IF);
        
        auto cond = parseExpression();
        auto thenBranch = parseBlock();
        
        std::unique_ptr<Stmt> elseBranch = nullptr;
        if (lexer_.match(TokenType::ELSE)) {
            if (lexer_.peek().type == TokenType::IF) {
                elseBranch = parseIfStmt();
            } else {
                elseBranch = parseBlock();
            }
        }
        
        return std::make_unique<IfStmt>(pos, std::move(cond), std::move(thenBranch), std::move(elseBranch));
    }
    
    // while_stmt := 'while' expr block
    std::unique_ptr<Stmt> parseWhileStmt() {
        auto pos = makePos();
        expect(TokenType::WHILE);
        
        auto cond = parseExpression();
        auto body = parseBlock();
        
        return std::make_unique<WhileStmt>(pos, std::move(cond), std::move(body));
    }
    
    // for_stmt := 'for' identifier 'in' expr ('..' | '..=') expr block
    std::unique_ptr<Stmt> parseForStmt() {
        auto pos = makePos();
        expect(TokenType::FOR);
        
        auto varTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        
        expect(TokenType::IN);
        
        auto start = parseExpression();
        
        bool inclusive = false;
        if (lexer_.match(TokenType::RANGE_INCL)) {
            inclusive = true;
        } else {
            expect(TokenType::RANGE);
        }
        
        auto end = parseExpression();
        
        auto body = parseBlock();
        
        return std::make_unique<ForStmt>(pos, varTok.text, std::move(start), std::move(end), inclusive, std::move(body));
    }
    
    // return_stmt := 'return' expr? ';'
    std::unique_ptr<Stmt> parseReturnStmt() {
        auto pos = makePos();
        expect(TokenType::RETURN);
        
        std::unique_ptr<Expr> value = nullptr;
        if (lexer_.peek().type != TokenType::SEMICOLON) {
            value = parseExpression();
        }
        
        expect(TokenType::SEMICOLON);
        
        return std::make_unique<ReturnStmt>(pos, std::move(value));
    }
    
    // break_stmt := 'break' ';'
    std::unique_ptr<Stmt> parseBreakStmt() {
        auto pos = makePos();
        expect(TokenType::BREAK);
        expect(TokenType::SEMICOLON);
        return std::make_unique<BreakStmt>(pos);
    }
    
    // continue_stmt := 'continue' ';'
    std::unique_ptr<Stmt> parseContinueStmt() {
        auto pos = makePos();
        expect(TokenType::CONTINUE);
        expect(TokenType::SEMICOLON);
        return std::make_unique<ContinueStmt>(pos);
    }
    
    // expr_stmt := expr ';'
    std::unique_ptr<Stmt> parseExprStmt() {
        auto pos = makePos();
        auto expr = parseExpression();
        expect(TokenType::SEMICOLON);
        return std::make_unique<ExprStmt>(pos, std::move(expr));
    }

    // give_stmt := 'give' expr identifier (':' '{' field_init_list '}')? ';'
    std::unique_ptr<Stmt> parseGiveStmt() {
        auto pos = makePos();
        expect(TokenType::GIVE);

        auto entity = parseExpression();

        auto compTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        std::string compName = compTok.text;

        std::vector<TemplateFieldInit> initFields;
        if (lexer_.match(TokenType::COLON)) {
            expect(TokenType::LBRACE);
            while (lexer_.peek().type != TokenType::RBRACE && !lexer_.isAtEnd()) {
                auto fieldTok = lexer_.peek();
                expect(TokenType::IDENTIFIER);
                expect(TokenType::COLON);
                auto value = parseExpression();
                initFields.push_back(TemplateFieldInit{fieldTok.text, std::move(value)});
                if (lexer_.peek().type == TokenType::COMMA) {
                    lexer_.next();
                }
            }
            expect(TokenType::RBRACE);
        }

        expect(TokenType::SEMICOLON);
        return std::make_unique<GiveStmt>(pos, std::move(entity), compName, std::move(initFields));
    }

    // tag_stmt := 'tag' expr 'as' identifier ';'
    std::unique_ptr<Stmt> parseTagStmt() {
        auto pos = makePos();
        expect(TokenType::TAG);

        auto entity = parseExpression();
        expect(TokenType::AS);

        auto traitTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);

        expect(TokenType::SEMICOLON);
        return std::make_unique<TagStmt>(pos, std::move(entity), traitTok.text);
    }

    // untag_stmt := 'untag' expr 'as' identifier ';'
    std::unique_ptr<Stmt> parseUntagStmt() {
        auto pos = makePos();
        expect(TokenType::UNTAG);

        auto entity = parseExpression();
        expect(TokenType::AS);

        auto traitTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);

        expect(TokenType::SEMICOLON);
        return std::make_unique<UntagStmt>(pos, std::move(entity), traitTok.text);
    }

    // destroy_stmt := 'destroy' expr ';'
    // For now, keep the original syntax: destroy e;
    std::unique_ptr<Stmt> parseDestroyStmt() {
        auto pos = makePos();
        expect(TokenType::DESTROY);

        auto entity = parseExpression();

        expect(TokenType::SEMICOLON);
        return std::make_unique<DestroyStmt>(pos, std::move(entity));
    }

    // query_stmt := 'query' identifier_list ('with' identifier_list)? 'as' identifier block
    std::unique_ptr<Stmt> parseQueryStmt() {
        auto pos = makePos();
        expect(TokenType::QUERY);

        // 解析 trait 列表
        std::vector<std::string> traits;
        auto traitTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);
        traits.push_back(traitTok.text);

        while (lexer_.match(TokenType::COMMA)) {
            auto nextTraitTok = lexer_.peek();
            expect(TokenType::IDENTIFIER);
            traits.push_back(nextTraitTok.text);
        }

        // 可选的 with 组件列表
        std::vector<std::string> withComps;
        if (lexer_.match(TokenType::WITH)) {
            auto compTok = lexer_.peek();
            expect(TokenType::IDENTIFIER);
            withComps.push_back(compTok.text);

            while (lexer_.match(TokenType::COMMA)) {
                auto nextCompTok = lexer_.peek();
                expect(TokenType::IDENTIFIER);
                withComps.push_back(nextCompTok.text);
            }
        }

        expect(TokenType::AS);

        auto varTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);

        auto body = parseBlock();

        return std::make_unique<QueryStmt>(pos, traits, withComps, varTok.text, std::move(body));
    }
    
    // expr := assign_expr
    std::unique_ptr<Expr> parseExpression() {
        return parseAssignExpr();
    }
    
    // range_expr := or_expr (('..' | '..=') or_expr)?
    std::unique_ptr<Expr> parseRangeExpr() {
        auto pos = makePos();
        auto lhs = parseOrExpr();
        
        // Note: Range is handled specially in for loops
        // Here we just return the expression
        return lhs;
    }
    
    // assign_expr := or_expr (('=' | '+=' | '-=') assign_expr)?
    std::unique_ptr<Expr> parseAssignExpr() {
        auto pos = makePos();
        auto lhs = parseOrExpr();
        
        if (lexer_.match(TokenType::ASSIGN)) {
            auto rhs = parseAssignExpr();
            return std::make_unique<AssignExpr>(pos, std::move(lhs), std::move(rhs));
        } else if (lexer_.match(TokenType::ADD_ASSIGN)) {
            // a += b 转换为 a = a + b
            auto rhs = parseAssignExpr();
            // 创建 lhs 的副本
            std::unique_ptr<Expr> lhsCopy = copyExpr(lhs.get());
            auto addExpr = std::make_unique<BinaryExpr>(pos, std::move(lhsCopy), BinOp::Add, std::move(rhs));
            return std::make_unique<AssignExpr>(pos, std::move(lhs), std::move(addExpr));
        } else if (lexer_.match(TokenType::SUB_ASSIGN)) {
            // a -= b 转换为 a = a - b
            auto rhs = parseAssignExpr();
            std::unique_ptr<Expr> lhsCopy = copyExpr(lhs.get());
            auto subExpr = std::make_unique<BinaryExpr>(pos, std::move(lhsCopy), BinOp::Sub, std::move(rhs));
            return std::make_unique<AssignExpr>(pos, std::move(lhs), std::move(subExpr));
        }
        
        return lhs;
    }
    
    // Helper to copy an expression
    std::unique_ptr<Expr> copyExpr(Expr* expr) {
        if (auto* id = dynamic_cast<Identifier*>(expr)) {
            return std::make_unique<Identifier>(id->pos, id->name);
        } else if (auto* member = dynamic_cast<MemberExpr*>(expr)) {
            return std::make_unique<MemberExpr>(member->pos, copyExpr(member->object.get()), member->member);
        } else if (auto* index = dynamic_cast<IndexExpr*>(expr)) {
            return std::make_unique<IndexExpr>(index->pos, copyExpr(index->object.get()), copyExpr(index->index.get()));
        }
        // For other types, return nullptr (should not happen in valid += -= usage)
        return nullptr;
    }
    
    // or_expr := and_expr ('||' and_expr)*
    std::unique_ptr<Expr> parseOrExpr() {
        auto pos = makePos();
        auto lhs = parseAndExpr();
        
        while (lexer_.match(TokenType::OR)) {
            auto rhs = parseAndExpr();
            lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Or, std::move(rhs));
        }
        
        return lhs;
    }
    
    // and_expr := eq_expr ('&&' eq_expr)*
    std::unique_ptr<Expr> parseAndExpr() {
        auto pos = makePos();
        auto lhs = parseEqExpr();
        
        while (lexer_.match(TokenType::AND)) {
            auto rhs = parseEqExpr();
            lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::And, std::move(rhs));
        }
        
        return lhs;
    }
    
    // eq_expr := rel_expr (('==' | '!=' | 'has') rel_expr)*
    std::unique_ptr<Expr> parseEqExpr() {
        auto pos = makePos();
        auto lhs = parseRelExpr();

        while (true) {
            if (lexer_.match(TokenType::EQ)) {
                auto rhs = parseRelExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Eq, std::move(rhs));
            } else if (lexer_.match(TokenType::NE)) {
                auto rhs = parseRelExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Ne, std::move(rhs));
            } else if (lexer_.match(TokenType::HAS)) {
                // e has Component 或 e has Trait
                auto nameTok = lexer_.peek();
                expect(TokenType::IDENTIFIER);
                // 暂时不知道是否是 trait，在代码生成阶段判断
                bool isTrait = false;  // 默认假设是组件
                bool negate = false;
                lhs = std::make_unique<HasExpr>(pos, std::move(lhs), nameTok.text, isTrait, negate);
            } else {
                break;
            }
        }

        return lhs;
    }
    
    // rel_expr := add_expr (('<' | '>' | '<=' | '>=') add_expr)*
    std::unique_ptr<Expr> parseRelExpr() {
        auto pos = makePos();
        auto lhs = parseAddExpr();
        
        while (true) {
            if (lexer_.match(TokenType::LT)) {
                auto rhs = parseAddExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Lt, std::move(rhs));
            } else if (lexer_.match(TokenType::GT)) {
                auto rhs = parseAddExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Gt, std::move(rhs));
            } else if (lexer_.match(TokenType::LE)) {
                auto rhs = parseAddExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Le, std::move(rhs));
            } else if (lexer_.match(TokenType::GE)) {
                auto rhs = parseAddExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Ge, std::move(rhs));
            } else {
                break;
            }
        }
        
        return lhs;
    }
    
    // add_expr := mul_expr (('+' | '-') mul_expr)*
    std::unique_ptr<Expr> parseAddExpr() {
        auto pos = makePos();
        auto lhs = parseMulExpr();
        
        while (true) {
            if (lexer_.match(TokenType::ADD)) {
                auto rhs = parseMulExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Add, std::move(rhs));
            } else if (lexer_.match(TokenType::SUB)) {
                auto rhs = parseMulExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Sub, std::move(rhs));
            } else {
                break;
            }
        }
        
        return lhs;
    }
    
    // mul_expr := unary_expr (('*' | '/' | '%') unary_expr)*
    std::unique_ptr<Expr> parseMulExpr() {
        auto pos = makePos();
        auto lhs = parseUnaryExpr();
        
        while (true) {
            if (lexer_.match(TokenType::MUL)) {
                auto rhs = parseUnaryExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Mul, std::move(rhs));
            } else if (lexer_.match(TokenType::DIV)) {
                auto rhs = parseUnaryExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Div, std::move(rhs));
            } else if (lexer_.match(TokenType::MOD)) {
                auto rhs = parseUnaryExpr();
                lhs = std::make_unique<BinaryExpr>(pos, std::move(lhs), BinOp::Mod, std::move(rhs));
            } else {
                break;
            }
        }
        
        return lhs;
    }
    
    // unary_expr := ('-' | '!' | '&') unary_expr | postfix_expr
    std::unique_ptr<Expr> parseUnaryExpr() {
        auto pos = makePos();

        if (lexer_.match(TokenType::SUB)) {
            auto operand = parseUnaryExpr();
            return std::make_unique<UnaryExpr>(pos, UnOp::Neg, std::move(operand));
        }
        if (lexer_.match(TokenType::NOT)) {
            // 检查是否是 !has
            if (lexer_.peek().type == TokenType::HAS) {
                lexer_.next();  // consume 'has'
                auto entity = parseUnaryExpr();
                auto nameTok = lexer_.peek();
                expect(TokenType::IDENTIFIER);
                bool isTrait = false;
                return std::make_unique<HasExpr>(pos, std::move(entity), nameTok.text, isTrait, true);
            }
            auto operand = parseUnaryExpr();
            return std::make_unique<UnaryExpr>(pos, UnOp::Not, std::move(operand));
        }
        if (lexer_.match(TokenType::MUL)) { // Dereference
            auto operand = parseUnaryExpr();
            return std::make_unique<UnaryExpr>(pos, UnOp::Deref, std::move(operand));
        }
        if (lexer_.match(TokenType::AND)) { // Address of
            auto operand = parseUnaryExpr();
            return std::make_unique<UnaryExpr>(pos, UnOp::AddrOf, std::move(operand));
        }
        
        return parsePostfixExpr();
    }
    
    // postfix_expr := primary_expr ('.' identifier | '(' arg_list ')' | '[' expr ']')*
    std::unique_ptr<Expr> parsePostfixExpr() {
        auto pos = makePos();
        auto expr = parsePrimaryExpr();
        
        while (true) {
            if (lexer_.match(TokenType::DOT)) {
                auto memberTok = lexer_.peek();
                expect(TokenType::IDENTIFIER);
                expr = std::make_unique<MemberExpr>(pos, std::move(expr), memberTok.text);
            } else if (lexer_.match(TokenType::LPAREN)) {
                auto args = parseArgList();
                expect(TokenType::RPAREN);
                expr = std::make_unique<CallExpr>(pos, std::move(expr), std::move(args));
            } else if (lexer_.match(TokenType::LBRACKET)) {
                auto index = parseExpression();
                expect(TokenType::RBRACKET);
                expr = std::make_unique<IndexExpr>(pos, std::move(expr), std::move(index));
            } else {
                break;
            }
        }
        
        return expr;
    }
    
    // primary_expr := identifier | integer | float | string | 'true' | 'false' | '(' expr ')' | query_expr
    std::unique_ptr<Expr> parsePrimaryExpr() {
        auto pos = makePos();
        auto tok = lexer_.peek();
        
        switch (tok.type) {
            case TokenType::IDENTIFIER:
            case TokenType::WORLD:
                // WORLD is a keyword but can also be used as an identifier (e.g., world parameter)
                lexer_.next();
                return std::make_unique<Identifier>(pos, tok.text);
                
            case TokenType::INTEGER_LITERAL:
                lexer_.next();
                return std::make_unique<IntegerLiteral>(pos, std::stoll(tok.text));
                
            case TokenType::FLOAT_LITERAL:
                lexer_.next();
                return std::make_unique<FloatLiteral>(pos, std::stod(tok.text));
                
            case TokenType::STRING_LITERAL:
                lexer_.next();
                // Remove quotes
                return std::make_unique<StringLiteral>(pos, tok.text.substr(1, tok.text.length() - 2));
                
            case TokenType::TRUE:
                lexer_.next();
                return std::make_unique<BoolLiteral>(pos, true);
                
            case TokenType::FALSE:
                lexer_.next();
                return std::make_unique<BoolLiteral>(pos, false);
                
            case TokenType::LPAREN: {
                lexer_.next();
                auto parenExpr = parseExpression();
                expect(TokenType::RPAREN);
                return parenExpr;
            }
                
            case TokenType::QUERY:
            case TokenType::QUERY_MUT:
                return parseQueryExpr();

            case TokenType::SPAWN:
                return parseSpawnExpr();

            case TokenType::LBRACKET:
                return parseArrayLiteralExpr();

            default:
                error("Unexpected token in expression: " + tok.text);
                return nullptr;
        }
    }

    // spawn_expr := 'spawn' identifier
    // Creates an entity from the specified template
    // Use 'give' statement to set component values after spawning
    std::unique_ptr<Expr> parseSpawnExpr() {
        auto pos = makePos();
        expect(TokenType::SPAWN);

        auto tmplTok = lexer_.peek();
        expect(TokenType::IDENTIFIER);

        return std::make_unique<SpawnExpr>(pos, tmplTok.text);
    }
    
    // query_expr := ('query' | 'query_mut') '<' type_list '>' '(' ')'
    std::unique_ptr<Expr> parseQueryExpr() {
        auto pos = makePos();
        bool isMut = lexer_.match(TokenType::QUERY_MUT);
        if (!isMut) {
            expect(TokenType::QUERY);
        }
        
        expect(TokenType::LT);
        auto types = parseTypeList();
        expect(TokenType::GT);
        
        expect(TokenType::LPAREN);
        expect(TokenType::RPAREN);
        
        return std::make_unique<QueryExpr>(pos, isMut, std::move(types));
    }

    // array_literal_expr := '[' (expr (',' expr)*)? ']'
    std::unique_ptr<Expr> parseArrayLiteralExpr() {
        auto pos = makePos();
        expect(TokenType::LBRACKET);
        
        std::vector<std::unique_ptr<Expr>> elements;
        
        // Empty array: []
        if (lexer_.peek().type == TokenType::RBRACKET) {
            lexer_.next();
            return std::make_unique<ArrayLiteralExpr>(pos, std::move(elements));
        }
        
        // Parse first element
        elements.push_back(parseExpression());
        
        // Parse remaining elements
        while (lexer_.match(TokenType::COMMA)) {
            elements.push_back(parseExpression());
        }
        
        expect(TokenType::RBRACKET);
        return std::make_unique<ArrayLiteralExpr>(pos, std::move(elements));
    }
    
    // type_list := type (',' type)*
    std::vector<Type> parseTypeList() {
        std::vector<Type> types;
        
        types.push_back(parseType());
        
        while (lexer_.match(TokenType::COMMA)) {
            types.push_back(parseType());
        }
        
        return types;
    }
    
    // arg_list := (expr (',' expr)*)?
    std::vector<std::unique_ptr<Expr>> parseArgList() {
        std::vector<std::unique_ptr<Expr>> args;
        
        if (lexer_.peek().type == TokenType::RPAREN) {
            return args;
        }
        
        args.push_back(parseExpression());
        
        while (lexer_.match(TokenType::COMMA)) {
            args.push_back(parseExpression());
        }
        
        return args;
    }
};

// SemanticChecker method implementations (inline for header-only)

inline void SemanticChecker::check(Package& pkg) {
    // Collect all defined names
    collectDefinitions(pkg);
    
    // Check trait definitions
    for (const auto& trait : pkg.traits) {
        checkTrait(*trait, pkg);
    }
    
    // Check template definitions
    for (const auto& tmpl : pkg.templates) {
        checkTemplate(*tmpl, pkg);
    }
    
    // Check systems
    for (const auto& sys : pkg.systems) {
        checkSystem(*sys, pkg);
    }
    
    // Check functions
    for (const auto& fn : pkg.functions) {
        checkFunction(*fn, pkg);
    }
    
    // Check export functions
    for (const auto& fn : pkg.exports) {
        checkFunction(*fn, pkg);
    }
    
    // Check dependencies
    for (const auto& dep : pkg.dependencies) {
        checkDependency(*dep, pkg);
    }
}

inline void SemanticChecker::collectDefinitions(Package& pkg) {
    for (const auto& d : pkg.datas) {
        dataNames.insert(d->name);
    }
    for (const auto& t : pkg.traits) {
        traitNames.insert(t->name);
    }
    for (const auto& tmpl : pkg.templates) {
        templateNames.insert(tmpl->name);
    }
    for (const auto& s : pkg.systems) {
        systemNames.insert(s->name);
    }
    for (const auto& f : pkg.functions) {
        functionNames.insert(f->name);
    }
    for (const auto& f : pkg.exports) {
        functionNames.insert(f->name);
    }
}

// 作用域管理
inline void SemanticChecker::pushScope() {
    varScopes.emplace_back();
}

inline void SemanticChecker::popScope() {
    if (!varScopes.empty()) {
        varScopes.pop_back();
    }
}

inline void SemanticChecker::addVariable(const std::string& name, bool isMutable, Position pos) {
    // 检查当前作用域是否已存在同名变量
    if (!varScopes.empty()) {
        if (varScopes.back().find(name) != varScopes.back().end()) {
            throw SemanticError("Variable '" + name + "' already defined in this scope", pos);
        }
        varScopes.back()[name] = isMutable;
    }
}

inline bool SemanticChecker::isVariableDefined(const std::string& name) {
    // 检查是否是函数参数
    if (currentFunctionParams.find(name) != currentFunctionParams.end()) {
        return true;
    }
    // 检查是否是系统参数
    if (currentSystemParams.find(name) != currentSystemParams.end()) {
        return true;
    }
    // 检查是否是 query 迭代变量
    if (currentQueryVars.find(name) != currentQueryVars.end()) {
        return true;
    }
    // 检查作用域链
    for (auto it = varScopes.rbegin(); it != varScopes.rend(); ++it) {
        if (it->find(name) != it->end()) {
            return true;
        }
    }
    return false;
}

inline bool SemanticChecker::isVariableMutable(const std::string& name) {
    // 检查是否是函数参数（默认可变）
    auto paramIt = currentFunctionParams.find(name);
    if (paramIt != currentFunctionParams.end()) {
        return paramIt->second;  // 返回参数的可变性
    }
    // 检查是否是系统参数（默认可变）
    if (currentSystemParams.find(name) != currentSystemParams.end()) {
        return true;
    }
    // 检查是否是 query 迭代变量（默认可变）
    if (currentQueryVars.find(name) != currentQueryVars.end()) {
        return true;
    }
    // 检查作用域链
    for (auto it = varScopes.rbegin(); it != varScopes.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            return varIt->second;  // 返回变量的可变性
        }
    }
    return false;  // 未定义的变量视为不可变
}

inline void SemanticChecker::checkTrait(TraitDecl& trait, Package& pkg) {
    for (const auto& dataName : trait.requiredData) {
        if (dataNames.find(dataName) == dataNames.end()) {
            throw SemanticError("Undefined data type '" + dataName + "' in trait '" + trait.name + "'", trait.pos);
        }
    }
}

inline void SemanticChecker::checkTemplate(TemplateDecl& tmpl, Package& pkg) {
    for (const auto& comp : tmpl.components) {
        if (dataNames.find(comp.componentName) == dataNames.end()) {
            throw SemanticError("Undefined component type '" + comp.componentName + "' in template '" + tmpl.name + "'", tmpl.pos);
        }
    }
    for (const auto& traitName : tmpl.tags) {
        if (traitNames.find(traitName) == traitNames.end()) {
            throw SemanticError("Undefined trait '" + traitName + "' in template '" + tmpl.name + "'", tmpl.pos);
        }
    }
}

inline void SemanticChecker::checkSystem(SystemDecl& sys, Package& pkg) {
    for (const auto& traitName : sys.forTraits) {
        if (traitNames.find(traitName) == traitNames.end()) {
            throw SemanticError("Undefined trait '" + traitName + "' in system '" + sys.name + "'", sys.pos);
        }
    }
    if (sys.body) {
        // 保存当前系统参数
        auto savedSystemParams = currentSystemParams;
        currentSystemParams.clear();
        // 添加系统参数（dt 等）
        for (const auto& param : sys.params) {
            currentSystemParams.insert(param.name);
        }
        // 添加组件访问（在 for Trait in system 中）
        if (sys.isTraitSystem && !sys.forTraits.empty()) {
            for (const auto& traitName : sys.forTraits) {
                auto traitIt = std::find_if(pkg.traits.begin(), pkg.traits.end(),
                    [&traitName](const auto& t) { return t->name == traitName; });
                if (traitIt != pkg.traits.end()) {
                    for (const auto& dataName : (*traitIt)->requiredData) {
                        currentSystemParams.insert(dataName);  // Position, Velocity 等
                    }
                }
            }
            // Add 'e' variable for current entity in trait systems
            currentSystemParams.insert("e");
        }
        
        pushScope();
        checkBlock(*sys.body, pkg);
        popScope();
        
        currentSystemParams = savedSystemParams;
    }
}

inline void SemanticChecker::checkFunction(FnDecl& fn, Package& pkg) {
    if (fn.body) {
        // 保存当前函数参数
        auto savedFunctionParams = currentFunctionParams;
        currentFunctionParams.clear();
        // 添加函数参数（函数参数默认可变）
        for (const auto& param : fn.params) {
            currentFunctionParams[param.name] = true;  // 默认可变
        }
        
        pushScope();
        checkBlock(*fn.body, pkg);
        popScope();
        
        currentFunctionParams = savedFunctionParams;
    }
}

inline void SemanticChecker::checkDependency(DependencyDecl& dep, Package& pkg) {
    if (systemNames.find(dep.before) == systemNames.end()) {
        throw SemanticError("Undefined system '" + dep.before + "' in dependency", dep.pos);
    }
    if (systemNames.find(dep.after) == systemNames.end()) {
        throw SemanticError("Undefined system '" + dep.after + "' in dependency", dep.pos);
    }
}

inline void SemanticChecker::checkBlock(Block& block, Package& pkg) {
    for (auto& stmt : block.statements) {
        checkStatement(*stmt, pkg);
    }
}

inline void SemanticChecker::checkStatement(Stmt& stmt, Package& pkg) {
    if (auto* give = dynamic_cast<GiveStmt*>(&stmt)) {
        checkGiveStmt(*give, pkg);
    } else if (auto* tag = dynamic_cast<TagStmt*>(&stmt)) {
        checkTagStmt(*tag, pkg);
    } else if (auto* untag = dynamic_cast<UntagStmt*>(&stmt)) {
        checkUntagStmt(*untag, pkg);
    } else if (auto* destroy = dynamic_cast<DestroyStmt*>(&stmt)) {
        checkDestroyStmt(*destroy, pkg);
    } else if (auto* query = dynamic_cast<QueryStmt*>(&stmt)) {
        checkQueryStmt(*query, pkg);
    } else if (auto* var = dynamic_cast<VarDecl*>(&stmt)) {
        checkVarDecl(*var, pkg);
    } else if (auto* expr = dynamic_cast<ExprStmt*>(&stmt)) {
        checkExpression(*expr->expr, pkg);
    } else if (auto* ifstmt = dynamic_cast<IfStmt*>(&stmt)) {
        checkExpression(*ifstmt->condition, pkg);
        checkStatement(*ifstmt->then_branch, pkg);
        if (ifstmt->else_branch) {
            checkStatement(*ifstmt->else_branch, pkg);
        }
    } else if (auto* whilestmt = dynamic_cast<WhileStmt*>(&stmt)) {
        checkExpression(*whilestmt->condition, pkg);
        checkStatement(*whilestmt->body, pkg);
    } else if (auto* forstmt = dynamic_cast<ForStmt*>(&stmt)) {
        checkForStmt(*forstmt, pkg);
    } else if (auto* forin = dynamic_cast<ForInStmt*>(&stmt)) {
        checkForInStmt(*forin, pkg);
    } else if (auto* ret = dynamic_cast<ReturnStmt*>(&stmt)) {
        if (ret->value) {
            checkExpression(*ret->value, pkg);
        }
    } else if (auto* block = dynamic_cast<Block*>(&stmt)) {
        pushScope();
        checkBlock(*block, pkg);
        popScope();
    }
}

inline void SemanticChecker::checkVarDecl(VarDecl& stmt, Package& pkg) {
    // 检查初始化表达式
    if (stmt.init) {
        checkExpression(*stmt.init, pkg);
    }
    // 添加变量到当前作用域，传递可变性信息
    addVariable(stmt.name, stmt.is_mut, stmt.pos);
}

inline void SemanticChecker::checkForStmt(ForStmt& stmt, Package& pkg) {
    checkExpression(*stmt.start, pkg);
    checkExpression(*stmt.end, pkg);
    // for 循环变量在新作用域中（默认可变）
    pushScope();
    addVariable(stmt.var, true, stmt.pos);
    checkStatement(*stmt.body, pkg);
    popScope();
}

inline void SemanticChecker::checkForInStmt(ForInStmt& stmt, Package& pkg) {
    checkExpression(*stmt.iterable, pkg);
    // for-in 循环变量在新作用域中（默认可变）
    pushScope();
    addVariable(stmt.var, true, stmt.pos);
    checkStatement(*stmt.body, pkg);
    popScope();
}

inline void SemanticChecker::checkGiveStmt(GiveStmt& stmt, Package& pkg) {
    checkExpression(*stmt.entity, pkg);
    if (dataNames.find(stmt.componentName) == dataNames.end()) {
        throw SemanticError("Undefined component type '" + stmt.componentName + "'", stmt.pos);
    }
}

inline void SemanticChecker::checkTagStmt(TagStmt& stmt, Package& pkg) {
    checkExpression(*stmt.entity, pkg);
    if (traitNames.find(stmt.traitName) == traitNames.end()) {
        throw SemanticError("Undefined trait '" + stmt.traitName + "'", stmt.pos);
    }
}

inline void SemanticChecker::checkUntagStmt(UntagStmt& stmt, Package& pkg) {
    checkExpression(*stmt.entity, pkg);
    if (traitNames.find(stmt.traitName) == traitNames.end()) {
        throw SemanticError("Undefined trait '" + stmt.traitName + "'", stmt.pos);
    }
}

inline void SemanticChecker::checkDestroyStmt(DestroyStmt& stmt, Package& pkg) {
    // destroy; (no entity) is only valid in trait systems - checked during code generation
    if (stmt.entity) {
        checkExpression(*stmt.entity, pkg);
    }
}

inline void SemanticChecker::checkQueryStmt(QueryStmt& stmt, Package& pkg) {
    for (const auto& traitName : stmt.traitNames) {
        if (traitNames.find(traitName) == traitNames.end()) {
            throw SemanticError("Undefined trait '" + traitName + "' in query", stmt.pos);
        }
    }
    for (const auto& compName : stmt.withComponents) {
        if (dataNames.find(compName) == dataNames.end()) {
            throw SemanticError("Undefined component type '" + compName + "' in query", stmt.pos);
        }
    }
    if (stmt.body) {
        // query 语句创建新作用域，并添加迭代变量和组件访问
        pushScope();
        addVariable(stmt.varName, true, stmt.pos);  // 迭代变量如 e（默认可变）
        
        // 保存当前的 query 变量
        auto savedQueryVars = currentQueryVars;
        currentQueryVars.clear();
        currentQueryVars.insert(stmt.varName);
        
        // 添加 withComponents 到可用变量（用于直接访问）
        for (const auto& compName : stmt.withComponents) {
            currentQueryVars.insert(compName);
        }
        
        // 从 trait 中添加组件
        for (const auto& traitName : stmt.traitNames) {
            auto traitIt = std::find_if(pkg.traits.begin(), pkg.traits.end(),
                [&traitName](const auto& t) { return t->name == traitName; });
            if (traitIt != pkg.traits.end()) {
                for (const auto& dataName : (*traitIt)->requiredData) {
                    currentQueryVars.insert(dataName);
                }
            }
        }
        
        checkBlock(*stmt.body, pkg);
        popScope();
        currentQueryVars = savedQueryVars;
    }
}

inline void SemanticChecker::checkIdentifier(Identifier& id, Package& pkg) {
    std::string name = id.name;
    
    // 检查是否是已定义的变量
    if (isVariableDefined(name)) {
        return;
    }
    
    // 检查是否是组件名（用于 Component.field 访问）
    if (dataNames.find(name) != dataNames.end()) {
        return;
    }
    
    // 检查是否是函数名（用于函数调用）
    if (functionNames.find(name) != functionNames.end()) {
        return;
    }
    
    // 检查是否是系统名（虽然不太可能在表达式中直接使用）
    if (systemNames.find(name) != systemNames.end()) {
        return;
    }
    
    // 特殊允许的名字
    if (name == "world" || name == "dt" || name == "e") {
        return;
    }
    
    throw SemanticError("Undefined identifier '" + name + "'", id.pos);
}

inline void SemanticChecker::checkExpression(Expr& expr, Package& pkg) {
    if (auto* id = dynamic_cast<Identifier*>(&expr)) {
        checkIdentifier(*id, pkg);
    } else if (auto* has = dynamic_cast<HasExpr*>(&expr)) {
        checkExpression(*has->entity, pkg);
        if (!has->isTrait) {
            if (dataNames.find(has->name) == dataNames.end()) {
                throw SemanticError("Undefined component type '" + has->name + "'", has->pos);
            }
        } else {
            if (traitNames.find(has->name) == traitNames.end()) {
                throw SemanticError("Undefined trait '" + has->name + "'", has->pos);
            }
        }
    } else if (auto* spawn = dynamic_cast<SpawnExpr*>(&expr)) {
        if (templateNames.find(spawn->templateName) == templateNames.end()) {
            throw SemanticError("Undefined template '" + spawn->templateName + "'", spawn->pos);
        }
    } else if (auto* member = dynamic_cast<MemberExpr*>(&expr)) {
        checkExpression(*member->object, pkg);
        // 额外检查：如果 object 是标识符，可能是组件访问
        if (auto* objId = dynamic_cast<Identifier*>(member->object.get())) {
            // 检查是否是组件名
            if (dataNames.find(objId->name) != dataNames.end()) {
                // Component.field - 这是合法的组件访问
                return;
            }
        }
    } else if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        checkExpression(*bin->left, pkg);
        checkExpression(*bin->right, pkg);
    } else if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        checkExpression(*un->operand, pkg);
    } else if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        // 检查函数调用
        if (auto* calleeId = dynamic_cast<Identifier*>(call->callee.get())) {
            if (functionNames.find(calleeId->name) == functionNames.end()) {
                throw SemanticError("Undefined function '" + calleeId->name + "'", calleeId->pos);
            }
        } else {
            checkExpression(*call->callee, pkg);
        }
        for (auto& arg : call->args) {
            checkExpression(*arg, pkg);
        }
    } else if (auto* assign = dynamic_cast<AssignExpr*>(&expr)) {
        // 检查赋值目标是否可变
        // 对于简单变量赋值（非成员访问），需要检查可变性
        if (auto* id = dynamic_cast<Identifier*>(assign->left.get())) {
            // 直接赋值给变量：检查变量是否可变
            if (!isVariableMutable(id->name)) {
                throw SemanticError("Cannot assign to immutable variable '" + id->name + "'", id->pos);
            }
        }
        // 对于成员访问（如 Position.x = 10），允许赋值，因为这是修改组件字段
        checkExpression(*assign->left, pkg);
        checkExpression(*assign->right, pkg);
    } else if (auto* index = dynamic_cast<IndexExpr*>(&expr)) {
        checkExpression(*index->object, pkg);
        checkExpression(*index->index, pkg);
    }
}

// Entry point
inline std::unique_ptr<Package> parse_source(std::string_view source, const std::string& packageName = "main") {
    Parser parser(source, packageName);
    return parser.parse();
}

} // namespace paani

#endif // PAANI_PARSER_RD_HPP

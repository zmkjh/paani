// paani_ast.hpp - Paani Abstract Syntax Tree
// Clean, modern AST design with visitor pattern support

#ifndef PAANI_AST_HPP
#define PAANI_AST_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace paani {

// ============================================================================
// Source Location
// ============================================================================
struct Location {
    uint32_t line = 1;
    uint32_t column = 1;
    uint32_t offset = 0;  // Byte offset in source

    Location() = default;
    Location(uint32_t l, uint32_t c, uint32_t o = 0)
        : line(l), column(c), offset(o) {}

    std::string toString() const {
        return std::to_string(line) + ":" + std::to_string(column);
    }
};

// ============================================================================
// Type System
// ============================================================================
enum class TypeKind : uint8_t {
    // Primitives
    Void,
    Bool,
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,
    String,
    Entity,

    // Composite
    Pointer,      // *T
    Array,        // [T; N]
    UserDefined,  // struct/trait name
    Function,     // fn(T) -> R
    ExternType,   // extern type T[N]
    Handle,       // handle Type with dtor
};

// Forward declarations for AST nodes
struct Expr;
struct Stmt;
struct Decl;
struct Type;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;
using TypePtr = std::unique_ptr<Type>;

// Type representation
struct Type {
    TypeKind kind;
    std::string name;      // For UserDefined: the name
    TypePtr base;          // For Pointer/Array: element type
    uint32_t arraySize;    // For Array: size (0 = dynamic/unknown)
    std::vector<TypePtr> funcParams;  // For Function: parameter types
    TypePtr funcReturn;    // For Function: return type
    std::string world;     // For Entity: the package world it belongs to (empty = current package)

    explicit Type(TypeKind k = TypeKind::Void)
        : kind(k), arraySize(0) {}

    // Factory methods
    static TypePtr void_() { return std::make_unique<Type>(TypeKind::Void); }
    static TypePtr bool_() { return std::make_unique<Type>(TypeKind::Bool); }
    static TypePtr i32() { return std::make_unique<Type>(TypeKind::I32); }
    static TypePtr f32() { return std::make_unique<Type>(TypeKind::F32); }
    static TypePtr string() { return std::make_unique<Type>(TypeKind::String); }
    static TypePtr entity() { return std::make_unique<Type>(TypeKind::Entity); }
    static TypePtr entityWithWorld(const std::string& pkgWorld) {
        auto t = std::make_unique<Type>(TypeKind::Entity);
        t->world = pkgWorld;
        return t;
    }
    static TypePtr userDefined(const std::string& name) {
        auto t = std::make_unique<Type>(TypeKind::UserDefined);
        t->name = name;
        return t;
    }
    static TypePtr pointer(TypePtr base) {
        auto t = std::make_unique<Type>(TypeKind::Pointer);
        t->base = std::move(base);
        return t;
    }
    static TypePtr array(TypePtr base, uint32_t size) {
        auto t = std::make_unique<Type>(TypeKind::Array);
        t->base = std::move(base);
        t->arraySize = size;
        return t;
    }

    // Convert to string representation
    std::string toString() const;
};

// ============================================================================
// Base AST Node
// ============================================================================
struct ASTNode {
    Location loc;

    explicit ASTNode(Location l = Location{}) : loc(l) {}
    virtual ~ASTNode() = default;

    // Visitor support
    virtual void accept(class ASTVisitor& visitor) = 0;
    virtual void acceptConst(class ConstASTVisitor& visitor) const = 0;
};

// ============================================================================
// Expressions
// ============================================================================
enum class BinOp : uint8_t {
    Add, Sub, Mul, Div, Mod,
    Eq, Ne, Lt, Gt, Le, Ge,
    And, Or,
    Assign, AddAssign, SubAssign,
    // Bitwise operators
    BitAnd, BitOr, BitXor, BitShl, BitShr
};

enum class UnOp : uint8_t {
    Neg, Not, Deref, AddrOf,
    // Bitwise NOT
    BitNot
};

struct Expr : ASTNode {
    using ASTNode::ASTNode;
    TypePtr type;  // Inferred or declared type
};

// Identifier: foo, bar, Position
struct IdentifierExpr : Expr {
    std::string name;

    IdentifierExpr(Location loc, std::string n)
        : Expr(loc), name(std::move(n)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Integer literal: 42, 100
struct IntegerExpr : Expr {
    int64_t value;

    IntegerExpr(Location loc, int64_t v)
        : Expr(loc), value(v) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Float literal: 3.14, 2.5
struct FloatExpr : Expr {
    double value;

    FloatExpr(Location loc, double v)
        : Expr(loc), value(v) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// String literal: "hello"
struct StringExpr : Expr {
    std::string value;

    StringExpr(Location loc, std::string v)
        : Expr(loc), value(std::move(v)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Template string: f"hello {name}"
struct FStringExpr : Expr {
    struct Part {
        bool isExpr;           // true = 表达式, false = 纯文本
        std::string text;      // 纯文本内容
        std::unique_ptr<Expr> expr;  // 表达式（如果是插值）
    };
    std::vector<Part> parts;

    FStringExpr(Location loc, std::vector<Part> p)
        : Expr(loc), parts(std::move(p)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Bool literal: true, false
struct BoolExpr : Expr {
    bool value;

    BoolExpr(Location loc, bool v)
        : Expr(loc), value(v) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Binary expression: a + b, x == y
struct BinaryExpr : Expr {
    BinOp op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(Location loc, BinOp o, ExprPtr l, ExprPtr r)
        : Expr(loc), op(o), left(std::move(l)), right(std::move(r)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Unary expression: -x, !flag
struct UnaryExpr : Expr {
    UnOp op;
    ExprPtr operand;

    UnaryExpr(Location loc, UnOp o, ExprPtr opd)
        : Expr(loc), op(o), operand(std::move(opd)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Function call: foo(a, b)
struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> args;

    CallExpr(Location loc, ExprPtr c, std::vector<ExprPtr> a)
        : Expr(loc), callee(std::move(c)), args(std::move(a)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Member access: entity.Position, foo.bar
struct MemberExpr : Expr {
    ExprPtr object;
    std::string member;

    MemberExpr(Location loc, ExprPtr o, std::string m)
        : Expr(loc), object(std::move(o)), member(std::move(m)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Array/Index access: arr[i]
struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;

    IndexExpr(Location loc, ExprPtr o, ExprPtr i)
        : Expr(loc), object(std::move(o)), index(std::move(i)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Spawn expression: spawn Player
struct SpawnExpr : Expr {
    std::string templateName;

    SpawnExpr(Location loc, std::string tmpl)
        : Expr(loc), templateName(std::move(tmpl)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Has expression: e has Position, e !has Trait
struct HasExpr : Expr {
    ExprPtr entity;
    std::string name;    // Component or trait name
    bool isTrait;        // true if checking trait
    bool negate;         // true if !has

    HasExpr(Location loc, ExprPtr e, std::string n, bool trait, bool neg = false)
        : Expr(loc), entity(std::move(e)), name(std::move(n)),
          isTrait(trait), negate(neg) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Array literal expression: [1, 2, 3]
struct ArrayLiteralExpr : Expr {
    std::vector<ExprPtr> elements;

    ArrayLiteralExpr(Location loc, std::vector<ExprPtr> elems = {})
        : Expr(loc), elements(std::move(elems)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// ============================================================================
// Statements
// ============================================================================
struct Stmt : ASTNode {
    using ASTNode::ASTNode;
};

// Block: { stmt1; stmt2; }
struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;

    BlockStmt(Location loc, std::vector<StmtPtr> s = {})
        : Stmt(loc), statements(std::move(s)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Variable declaration: let x: i32 = 0;
struct VarDeclStmt : Stmt {
    std::string name;
    TypePtr type;
    bool isMutable;
    ExprPtr init;

    VarDeclStmt(Location loc, std::string n, TypePtr t, bool mut, ExprPtr i)
        : Stmt(loc), name(std::move(n)), type(std::move(t)),
          isMutable(mut), init(std::move(i)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Expression statement: foo();
struct ExprStmt : Stmt {
    ExprPtr expr;

    ExprStmt(Location loc, ExprPtr e)
        : Stmt(loc), expr(std::move(e)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// If statement: if cond { ... } else { ... }
struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;  // nullptr if no else

    IfStmt(Location loc, ExprPtr c, StmtPtr t, StmtPtr e = nullptr)
        : Stmt(loc), condition(std::move(c)), thenBranch(std::move(t)),
          elseBranch(std::move(e)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// While statement: while cond { ... }
struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;

    WhileStmt(Location loc, ExprPtr c, StmtPtr b)
        : Stmt(loc), condition(std::move(c)), body(std::move(b)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Return statement: return expr;
struct ReturnStmt : Stmt {
    ExprPtr value;  // nullptr for bare return

    ReturnStmt(Location loc, ExprPtr v = nullptr)
        : Stmt(loc), value(std::move(v)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Break statement: break;
struct BreakStmt : Stmt {
    BreakStmt(Location loc) : Stmt(loc) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Continue statement: continue;
struct ContinueStmt : Stmt {
    ContinueStmt(Location loc) : Stmt(loc) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Exit statement: exit; (exits the game loop in .paaniworld files)
struct ExitStmt : Stmt {
    ExitStmt(Location loc) : Stmt(loc) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Give statement: give e Component; or give e Component: {x: 0}
struct GiveStmt : Stmt {
    ExprPtr entity;
    std::string componentName;
    std::vector<std::pair<std::string, ExprPtr>> initFields;

    GiveStmt(Location loc, ExprPtr e, std::string comp,
             std::vector<std::pair<std::string, ExprPtr>> init = {})
        : Stmt(loc), entity(std::move(e)), componentName(std::move(comp)),
          initFields(std::move(init)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Tag statement: tag e as Trait;
struct TagStmt : Stmt {
    ExprPtr entity;
    std::string traitName;

    TagStmt(Location loc, ExprPtr e, std::string t)
        : Stmt(loc), entity(std::move(e)), traitName(std::move(t)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Untag statement: untag e as Trait;
struct UntagStmt : Stmt {
    ExprPtr entity;
    std::string traitName;

    UntagStmt(Location loc, ExprPtr e, std::string t)
        : Stmt(loc), entity(std::move(e)), traitName(std::move(t)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Destroy statement: destroy e; or destroy;
struct DestroyStmt : Stmt {
    ExprPtr entity;  // nullptr means destroy current entity in trait system

    DestroyStmt(Location loc, ExprPtr e = nullptr)
        : Stmt(loc), entity(std::move(e)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Query statement: query Trait as e { ... }
struct QueryStmt : Stmt {
    std::vector<std::string> traitNames;
    std::vector<std::string> withComponents;
    std::string varName;
    std::unique_ptr<BlockStmt> body;

    QueryStmt(Location loc, std::vector<std::string> traits,
              std::vector<std::string> comps, std::string var,
              std::unique_ptr<BlockStmt> b)
        : Stmt(loc), traitNames(std::move(traits)),
          withComponents(std::move(comps)), varName(std::move(var)),
          body(std::move(b)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Lock statement: lock systemName; or lock package.systemName;
struct LockStmt : Stmt {
    std::vector<std::string> systemPath;  // e.g., ["engine", "gravitySystem"]

    LockStmt(Location loc, std::vector<std::string> path)
        : Stmt(loc), systemPath(std::move(path)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Unlock statement: unlock systemName; or unlock package.systemName;
struct UnlockStmt : Stmt {
    std::vector<std::string> systemPath;  // e.g., ["engine", "gravitySystem"]

    UnlockStmt(Location loc, std::vector<std::string> path)
        : Stmt(loc), systemPath(std::move(path)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// ============================================================================
// Declarations
// ============================================================================
struct Decl : ASTNode {
    using ASTNode::ASTNode;
    std::string name;
};

// Field in struct/component
struct Field {
    std::string name;
    TypePtr type;
    ExprPtr defaultValue;
};

// Parameter in function
struct Param {
    std::string name;
    TypePtr type;
    bool isMutable;
};

// Data declaration: data Position { x: f32, y: f32 }
// Can be prefixed with 'export' to make it accessible from other packages
struct DataDecl : Decl {
    std::vector<Field> fields;
    bool isExport = false;  // Whether this data is exported for cross-package access

    DataDecl(Location loc, std::string n, std::vector<Field> f, bool exported = false)
        : Decl(loc), fields(std::move(f)), isExport(exported) {
        name = std::move(n);
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Trait declaration: trait Movable [Position, Velocity];
// Can be prefixed with 'export' to make it accessible from other packages
struct TraitDecl : Decl {
    std::vector<std::string> requiredData;  // Empty = pure tag
    bool isExport = false;  // Whether this trait is exported for cross-package access

    TraitDecl(Location loc, std::string n, std::vector<std::string> reqs, bool exported = false)
        : Decl(loc), requiredData(std::move(reqs)), isExport(exported) {
        name = std::move(n);
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Template component initialization
struct TemplateComponentInit {
    std::string componentName;
    std::vector<std::pair<std::string, ExprPtr>> fieldValues;
};

// Template declaration: template Player { Position: {x: 0}, tags: [Movable] }
// Can be prefixed with 'export' to make it accessible from other packages
struct TemplateDecl : Decl {
    std::vector<TemplateComponentInit> components;
    std::vector<std::string> tags;
    bool isExport = false;  // Whether this template is exported for cross-package access

    TemplateDecl(Location loc, std::string n,
                 std::vector<TemplateComponentInit> comps,
                 std::vector<std::string> t,
                 bool exported = false)
        : Decl(loc), components(std::move(comps)), tags(std::move(t)), isExport(exported) {
        name = std::move(n);
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// System declaration: system name(params) { ... } or for Trait in name(params) as entityVar { ... }
// Can be prefixed with #batch for SoA optimization hint
struct SystemDecl : Decl {
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;
    std::vector<std::string> forTraits;  // Non-empty = trait system
    bool isBatch;                        // #batch attribute for SoA optimization
    std::string entityVarName;           // Explicit entity variable name (default: "e")

    SystemDecl(Location loc, std::string n, std::vector<Param> p,
               std::unique_ptr<BlockStmt> b, bool batch = false)
        : Decl(loc), params(std::move(p)), body(std::move(b)), isBatch(batch), entityVarName("e") {
        name = std::move(n);
    }

    SystemDecl(Location loc, std::vector<std::string> traits, std::string n,
               std::vector<Param> p, std::unique_ptr<BlockStmt> b, bool batch = false)
        : Decl(loc), params(std::move(p)), body(std::move(b)),
          forTraits(std::move(traits)), isBatch(batch), entityVarName("e") {
        name = std::move(n);
    }

    SystemDecl(Location loc, std::vector<std::string> traits, std::string n,
               std::vector<Param> p, std::unique_ptr<BlockStmt> b, 
               std::string entityVar, bool batch = false)
        : Decl(loc), params(std::move(p)), body(std::move(b)),
          forTraits(std::move(traits)), isBatch(batch), entityVarName(std::move(entityVar)) {
        name = std::move(n);
    }

    bool isTraitSystem() const { return !forTraits.empty(); }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Dependency declaration: depend A -> B -> C -> D;
struct DependencyDecl : Decl {
    std::vector<std::string> systems;  // Chain of systems: A -> B -> C means {A, B, C}

    DependencyDecl(Location loc, std::vector<std::string> sys)
        : Decl(loc), systems(std::move(sys)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Function declaration: fn name(params) -> type { ... }
struct FnDecl : Decl {
    std::vector<Param> params;
    TypePtr returnType;
    std::unique_ptr<BlockStmt> body;
    bool isExport;

    FnDecl(Location loc, std::string n, std::vector<Param> p,
           TypePtr rt, std::unique_ptr<BlockStmt> b)
        : Decl(loc), params(std::move(p)), returnType(std::move(rt)),
          body(std::move(b)), isExport(false) {
        name = std::move(n);
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// External function declaration: extern fn name(params): returnType;
struct ExternFnDecl : Decl {
    std::vector<Param> params;
    TypePtr returnType;
    bool isVariadic;  // true if has ... (varargs)

    ExternFnDecl(Location loc, std::string n, std::vector<Param> p,
                 TypePtr rt, bool variadic = false)
        : Decl(loc), params(std::move(p)), returnType(std::move(rt)),
          isVariadic(variadic) {
        name = std::move(n);
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// External type declaration: extern type Name[N];
// Declares an opaque C type with known size N bytes
struct ExternTypeDecl : Decl {
    uint32_t size;  // Size in bytes

    ExternTypeDecl(Location loc, std::string n, uint32_t s)
        : Decl(loc), size(s) {
        name = std::move(n);
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Handle type declaration: handle Name with destructor;
// Declares a managed handle type with automatic destructor call
// Can be prefixed with 'export' to make it accessible from other packages
struct HandleDecl : Decl {
    std::string dtor;  // Destructor function name
    bool isExport = false;  // Whether this handle type is exported for cross-package access

    HandleDecl(Location loc, std::string n, std::string d, bool exported = false)
        : Decl(loc), dtor(std::move(d)), isExport(exported) {
        name = std::move(n);
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Use declaration: use package.name; (loads full ECS package, systems default to locked)
struct UseDecl : Decl {
    std::vector<std::string> packagePath;  // e.g., ["engine", "physics"]
    std::string alias;  // empty if no "as" clause
    bool onlyUtils;  // true for non-entry files (equivalent to old import behavior)

    UseDecl(Location loc, std::vector<std::string> path, std::string a = "", bool utilsOnly = false)
        : Decl(loc), packagePath(std::move(path)), alias(std::move(a)), onlyUtils(utilsOnly) {
        name = this->packagePath.empty() ? "" : this->packagePath.back();
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// Export declaration: export TypeName; or export funcName;
// Exports extern types/functions with package-prefixed names for cross-package use
struct ExportDecl : Decl {
    bool isType;  // true if exporting an extern type, false if exporting an extern function

    ExportDecl(Location loc, std::string n, bool type)
        : Decl(loc), isType(type) {
        name = std::move(n);
    }

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// ============================================================================
// Package (Top-level)
// ============================================================================
struct Package : ASTNode {
    std::string name;
    std::vector<std::unique_ptr<UseDecl>> uses;
    std::vector<std::unique_ptr<ExternFnDecl>> externFunctions;
    std::vector<std::unique_ptr<ExternTypeDecl>> externTypes;
    std::vector<std::unique_ptr<HandleDecl>> handles;
    std::vector<std::unique_ptr<ExportDecl>> exports;
    std::vector<std::unique_ptr<DataDecl>> datas;
    std::vector<std::unique_ptr<TraitDecl>> traits;
    std::vector<std::unique_ptr<TemplateDecl>> templates;
    std::vector<std::unique_ptr<SystemDecl>> systems;
    std::vector<std::unique_ptr<DependencyDecl>> dependencies;
    std::vector<std::unique_ptr<FnDecl>> functions;
    
    // For entry point file: global statements that will be compiled into main()
    std::vector<StmtPtr> globalStatements;
    bool isEntryPoint = false;

    Package(Location loc, std::string n)
        : ASTNode(loc), name(std::move(n)) {}

    void accept(ASTVisitor& visitor) override;
    void acceptConst(ConstASTVisitor& visitor) const override;
};

// ============================================================================
// Visitor Pattern
// ============================================================================
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // Expressions
    virtual void visit(IdentifierExpr& node) = 0;
    virtual void visit(IntegerExpr& node) = 0;
    virtual void visit(FloatExpr& node) = 0;
    virtual void visit(StringExpr& node) = 0;
    virtual void visit(FStringExpr& node) = 0;
    virtual void visit(BoolExpr& node) = 0;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(CallExpr& node) = 0;
    virtual void visit(MemberExpr& node) = 0;
    virtual void visit(IndexExpr& node) = 0;
    virtual void visit(SpawnExpr& node) = 0;
    virtual void visit(HasExpr& node) = 0;
    virtual void visit(ArrayLiteralExpr& node) = 0;

    // Statements
    virtual void visit(BlockStmt& node) = 0;
    virtual void visit(VarDeclStmt& node) = 0;
    virtual void visit(ExprStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;
    virtual void visit(BreakStmt& node) = 0;
    virtual void visit(ContinueStmt& node) = 0;
    virtual void visit(ExitStmt& node) = 0;
    virtual void visit(GiveStmt& node) = 0;
    virtual void visit(TagStmt& node) = 0;
    virtual void visit(UntagStmt& node) = 0;
    virtual void visit(DestroyStmt& node) = 0;
    virtual void visit(QueryStmt& node) = 0;
    virtual void visit(LockStmt& node) = 0;
    virtual void visit(UnlockStmt& node) = 0;

    // Declarations
    virtual void visit(DataDecl& node) = 0;
    virtual void visit(TraitDecl& node) = 0;
    virtual void visit(TemplateDecl& node) = 0;
    virtual void visit(SystemDecl& node) = 0;
    virtual void visit(DependencyDecl& node) = 0;
    virtual void visit(FnDecl& node) = 0;
    virtual void visit(ExternFnDecl& node) = 0;
    virtual void visit(ExternTypeDecl& node) = 0;
    virtual void visit(HandleDecl& node) = 0;
    virtual void visit(UseDecl& node) = 0;
    virtual void visit(ExportDecl& node) = 0;

    // Top-level
    virtual void visit(Package& node) = 0;
};

class ConstASTVisitor {
public:
    virtual ~ConstASTVisitor() = default;

    // Expressions
    virtual void visit(const IdentifierExpr& node) = 0;
    virtual void visit(const IntegerExpr& node) = 0;
    virtual void visit(const FloatExpr& node) = 0;
    virtual void visit(const StringExpr& node) = 0;
    virtual void visit(const FStringExpr& node) = 0;
    virtual void visit(const BoolExpr& node) = 0;
    virtual void visit(const BinaryExpr& node) = 0;
    virtual void visit(const UnaryExpr& node) = 0;
    virtual void visit(const CallExpr& node) = 0;
    virtual void visit(const MemberExpr& node) = 0;
    virtual void visit(const IndexExpr& node) = 0;
    virtual void visit(const SpawnExpr& node) = 0;
    virtual void visit(const HasExpr& node) = 0;
    virtual void visit(const ArrayLiteralExpr& node) = 0;

    // Statements
    virtual void visit(const BlockStmt& node) = 0;
    virtual void visit(const VarDeclStmt& node) = 0;
    virtual void visit(const ExprStmt& node) = 0;
    virtual void visit(const IfStmt& node) = 0;
    virtual void visit(const WhileStmt& node) = 0;
    virtual void visit(const ReturnStmt& node) = 0;
    virtual void visit(const BreakStmt& node) = 0;
    virtual void visit(const ContinueStmt& node) = 0;
    virtual void visit(const ExitStmt& node) = 0;
    virtual void visit(const GiveStmt& node) = 0;
    virtual void visit(const TagStmt& node) = 0;
    virtual void visit(const UntagStmt& node) = 0;
    virtual void visit(const DestroyStmt& node) = 0;
    virtual void visit(const QueryStmt& node) = 0;
    virtual void visit(const LockStmt& node) = 0;
    virtual void visit(const UnlockStmt& node) = 0;

    // Declarations
    virtual void visit(const DataDecl& node) = 0;
    virtual void visit(const TraitDecl& node) = 0;
    virtual void visit(const TemplateDecl& node) = 0;
    virtual void visit(const SystemDecl& node) = 0;
    virtual void visit(const DependencyDecl& node) = 0;
    virtual void visit(const FnDecl& node) = 0;
    virtual void visit(const ExternFnDecl& node) = 0;
    virtual void visit(const ExternTypeDecl& node) = 0;
    virtual void visit(const HandleDecl& node) = 0;
    virtual void visit(const UseDecl& node) = 0;
    virtual void visit(const ExportDecl& node) = 0;

    // Top-level
    virtual void visit(const Package& node) = 0;
};

// ============================================================================
// Implementations
// ============================================================================
inline std::string Type::toString() const {
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
        case TypeKind::Pointer: return "*" + (base ? base->toString() : "?");
        case TypeKind::Array: return "[" + (base ? base->toString() : "?") + "; " + std::to_string(arraySize) + "]";
        case TypeKind::UserDefined: return name;
        case TypeKind::Function: return "fn(...)";
    }
    return "unknown";
}

// Accept implementations
#define ACCEPT_IMPL(Class) \
    inline void Class::accept(ASTVisitor& visitor) { visitor.visit(*this); } \
    inline void Class::acceptConst(ConstASTVisitor& visitor) const { visitor.visit(*this); }

ACCEPT_IMPL(IdentifierExpr)
ACCEPT_IMPL(IntegerExpr)
ACCEPT_IMPL(FloatExpr)
ACCEPT_IMPL(StringExpr)
ACCEPT_IMPL(BoolExpr)
ACCEPT_IMPL(BinaryExpr)
ACCEPT_IMPL(UnaryExpr)
ACCEPT_IMPL(CallExpr)
ACCEPT_IMPL(MemberExpr)
ACCEPT_IMPL(IndexExpr)
ACCEPT_IMPL(SpawnExpr)
ACCEPT_IMPL(HasExpr)
ACCEPT_IMPL(ArrayLiteralExpr)
ACCEPT_IMPL(BlockStmt)
ACCEPT_IMPL(VarDeclStmt)
ACCEPT_IMPL(ExprStmt)
ACCEPT_IMPL(IfStmt)
ACCEPT_IMPL(WhileStmt)
ACCEPT_IMPL(ReturnStmt)
ACCEPT_IMPL(BreakStmt)
ACCEPT_IMPL(ContinueStmt)
ACCEPT_IMPL(ExitStmt)
ACCEPT_IMPL(GiveStmt)
ACCEPT_IMPL(TagStmt)
ACCEPT_IMPL(UntagStmt)
ACCEPT_IMPL(DestroyStmt)
ACCEPT_IMPL(QueryStmt)
ACCEPT_IMPL(LockStmt)
ACCEPT_IMPL(UnlockStmt)
ACCEPT_IMPL(DataDecl)
ACCEPT_IMPL(TraitDecl)
ACCEPT_IMPL(TemplateDecl)
ACCEPT_IMPL(SystemDecl)
ACCEPT_IMPL(DependencyDecl)
ACCEPT_IMPL(FnDecl)
ACCEPT_IMPL(ExternFnDecl)
ACCEPT_IMPL(ExternTypeDecl)
ACCEPT_IMPL(HandleDecl)
ACCEPT_IMPL(UseDecl)
ACCEPT_IMPL(ExportDecl)
ACCEPT_IMPL(Package)

// FStringExpr accept implementation (defined after ASTVisitor)
inline void FStringExpr::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}
inline void FStringExpr::acceptConst(ConstASTVisitor& visitor) const {
    visitor.visit(*this);
}

#undef ACCEPT_IMPL

} // namespace paani

#endif // PAANI_AST_HPP

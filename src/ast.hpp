#pragma once
#include "token.hpp"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>

// -----------------------------------------------------------------------
// Oberon-07 Abstract Syntax Tree
//
// Naming mirrors the grammar productions.  Every node stores its source
// location for diagnostics.  Ownership is always unique_ptr.
// -----------------------------------------------------------------------

// ---- Forward declarations ----------------------------------------------
struct Expr;
struct Stmt;
struct TypeExpr;
struct Designator;

using ExprPtr     = std::unique_ptr<Expr>;
using StmtPtr     = std::unique_ptr<Stmt>;
using TypePtr     = std::unique_ptr<TypeExpr>;
using DesignPtr   = std::unique_ptr<Designator>;

// -----------------------------------------------------------------------
// Selectors  (used inside Designator)
// -----------------------------------------------------------------------
struct FieldSelector  { std::string ident; SourceLoc loc; };
struct IndexSelector  { ExprPtr index;      SourceLoc loc; };
struct DerefSelector  {                     SourceLoc loc; };
struct TypeGuardSelector { std::string qualident; std::string module; SourceLoc loc; };

using Selector = std::variant<FieldSelector, IndexSelector,
                              DerefSelector, TypeGuardSelector>;

// -----------------------------------------------------------------------
// Designator  (qualident {selector})
// -----------------------------------------------------------------------
struct Designator {
    std::string module;   // empty if unqualified
    std::string ident;
    std::vector<Selector> selectors;
    SourceLoc loc;
};

// -----------------------------------------------------------------------
// Expressions
// -----------------------------------------------------------------------
enum class UnaryOp  { Plus, Minus, Not };
enum class BinaryOp { Add, Sub, Mul, Div, IDiv, IMod, And, Or,
                      Eq, Neq, Lt, Leq, Gt, Geq, In, Is };

struct Expr {
    SourceLoc loc;
    virtual ~Expr() = default;
};

struct IntLitExpr : Expr {
    std::string raw;   // keep raw text; value parsing is for semantic phase
};

struct RealLitExpr : Expr {
    std::string raw;
};

struct StringLitExpr : Expr {
    std::string raw;   // includes delimiters or X suffix
};

struct NilExpr   : Expr {};
struct TrueExpr  : Expr {};
struct FalseExpr : Expr {};

// set = "{" [element {"," element}] "}"
// element = expression [".." expression]
struct SetElement {
    ExprPtr lo;
    ExprPtr hi;   // nullptr if not a range
};
struct SetExpr : Expr {
    std::vector<SetElement> elements;
};

struct DesignatorExpr : Expr {
    DesignPtr desig;
    // ActualParameters (nullptr = plain variable reference, not a call)
    std::optional<std::vector<ExprPtr>> args;
};

struct UnaryExpr : Expr {
    UnaryOp  op;
    ExprPtr  operand;
};

struct BinaryExpr : Expr {
    BinaryOp op;
    ExprPtr  left, right;
};

// -----------------------------------------------------------------------
// Type expressions
// -----------------------------------------------------------------------
struct TypeExpr {
    SourceLoc loc;
    virtual ~TypeExpr() = default;
};

struct QualIdentType : TypeExpr {
    std::string module;   // empty if unqualified
    std::string ident;
};

struct ArrayTypeExpr : TypeExpr {
    std::vector<ExprPtr> lengths;   // ARRAY l1, l2, ... OF type
    TypePtr elemType;
};

// FieldList = IdentList ":" type
struct FieldList {
    std::vector<std::string> names;
    TypePtr type;
    SourceLoc loc;
};

struct RecordTypeExpr : TypeExpr {
    std::string baseModule;   // from BaseType (may be empty)
    std::string baseIdent;
    std::vector<FieldList> fields;
};

struct PointerTypeExpr : TypeExpr {
    TypePtr base;
};

struct FPSection {
    bool isVar = false;
    std::vector<std::string> names;
    TypePtr formalType;
    SourceLoc loc;
};

struct FormalParameters {
    std::vector<FPSection> sections;
    std::string retModule;   // empty if unqualified
    std::string retIdent;    // empty if no return type
    SourceLoc loc;
};

struct ProcedureTypeExpr : TypeExpr {
    std::optional<FormalParameters> params;
};

// -----------------------------------------------------------------------
// Statements
// -----------------------------------------------------------------------
struct Stmt {
    SourceLoc loc;
    virtual ~Stmt() = default;
};

using StmtSeq = std::vector<StmtPtr>;

struct AssignStmt : Stmt {
    DesignPtr target;
    ExprPtr   value;
};

struct ProcCallStmt : Stmt {
    DesignPtr desig;
    std::optional<std::vector<ExprPtr>> args;
};

struct IfBranch {
    ExprPtr   cond;
    StmtSeq   body;
};
struct IfStmt : Stmt {
    std::vector<IfBranch> branches;   // first = IF, rest = ELSIF
    std::optional<StmtSeq> elseBranch;
};

struct CaseLabelRange {
    ExprPtr lo;
    ExprPtr hi;   // nullptr if not ".." range
};
struct CaseArm {
    std::vector<CaseLabelRange> labels;
    StmtSeq body;
    SourceLoc loc;
};
struct CaseStmt : Stmt {
    ExprPtr expr;
    std::vector<CaseArm> arms;
};

struct WhileBranch {
    ExprPtr cond;
    StmtSeq body;
};
struct WhileStmt : Stmt {
    std::vector<WhileBranch> branches;   // first = WHILE, rest = ELSIF
};

struct RepeatStmt : Stmt {
    StmtSeq body;
    ExprPtr until;
};

struct ForStmt : Stmt {
    std::string var;
    ExprPtr     from, to;
    ExprPtr     by;   // nullptr if BY omitted
    StmtSeq     body;
};

// -----------------------------------------------------------------------
// Declarations
// -----------------------------------------------------------------------
struct ConstDecl {
    std::string name;
    bool exported = false;
    ExprPtr value;
    SourceLoc loc;
};

struct TypeDecl {
    std::string name;
    bool exported = false;
    TypePtr type;
    SourceLoc loc;
};

struct VarDecl {
    std::vector<std::string> names;
    std::vector<bool>        exported;
    TypePtr type;
    SourceLoc loc;
};

struct ProcDecl {
    std::string name;
    bool exported = false;
    std::optional<FormalParameters> params;
    // Body
    std::vector<ConstDecl> consts;
    std::vector<TypeDecl>  types;
    std::vector<VarDecl>   vars;
    std::vector<std::shared_ptr<ProcDecl>> procs;
    StmtSeq                body;
    ExprPtr                returnExpr;   // nullptr if absent
    SourceLoc loc;
};

struct DeclarationSequence {
    std::vector<ConstDecl> consts;
    std::vector<TypeDecl>  types;
    std::vector<VarDecl>   vars;
    std::vector<std::shared_ptr<ProcDecl>> procs;
};

// -----------------------------------------------------------------------
// Module (top-level unit)
// -----------------------------------------------------------------------
struct ImportEntry {
    std::string alias;    // empty → same as name
    std::string name;
    SourceLoc loc;
};

struct Module {
    std::string name;
    std::vector<ImportEntry> imports;
    DeclarationSequence decls;
    StmtSeq body;
    SourceLoc loc;
};

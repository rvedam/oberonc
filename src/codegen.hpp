#pragma once
#include "ast.hpp"
#include "sema.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

#include <set>
#include <span>
#include <string>
#include <memory>
#include <stdexcept>
#include <unordered_map>

// -----------------------------------------------------------------------
// Oberon-07 → LLVM IR code generator
//
// Usage:
//   CodeGen cg("Hello");
//   cg.generate(parsedModule);
//   cg.writeIR("Hello.ll");
//
// The module body (BEGIN … END Mod.) becomes void oberon_main().
// Each procedure P in module M becomes @M_P.
// Module-level variables become LLVM globals.
// -----------------------------------------------------------------------

class CodeGen {
public:
    explicit CodeGen(const std::string& moduleName);

    void generate(const Module& mod);
    void writeIR(const std::string& path);
    void writeIR(llvm::raw_ostream& os);
    void writeObject(const std::string& path); // emit native object file
    void setModulePaths(std::span<const std::string> paths) {
        modulePaths_.assign(paths.begin(), paths.end());
    }
    void setModuleInitOnly(bool v) { moduleInitOnly_ = v; }
    // Project Oberon ABI: INTEGER and SET are 32-bit (storage and arithmetic),
    // integer literals wrap to 32 bits, LSL/ASR/ROR use RISC5 semantics.
    void setInt32(bool v) { int32_ = v; }

    // Names of user modules that were successfully loaded (alias → modName)
    [[nodiscard]] const std::vector<std::pair<std::string,std::string>>& loadedUserModules() const {
        return loadedUserModules_;
    }

private:
    // ---- LLVM context ----
    llvm::LLVMContext                ctx_;
    std::unique_ptr<llvm::Module>    llvmMod_;
    std::unique_ptr<llvm::IRBuilder<>> b_;
    std::string                      modName_;

    // ---- Semantic state ----
    std::set<std::string>                   importedModules_;
    std::unordered_map<std::string, OberonTypePtr>    typeTable_;
    std::unordered_map<OberonType*, llvm::Type*>      llvmTypeCache_;
    // Keeps every OberonType we've cached alive so llvmTypeCache_ keys stay valid.
    // Without this, short-lived types (proc-local vars, params) can be freed and
    // their address reused, causing stale cache hits with the wrong LLVM type.
    std::vector<OberonTypePtr>                         liveTypes_;

    // ---- Module search paths (for resolving imports) ----
    std::vector<std::string>                modulePaths_;
    std::vector<std::pair<std::string,std::string>> loadedUserModules_; // (alias, modName)
    bool                                    moduleInitOnly_ = false; // generate ModName_init instead of oberon_main
    bool                                    int32_ = false;          // 32-bit INTEGER/SET (see setInt32)
    std::string                             sysAlias_;  // alias used to import SYSTEM (usually "SYSTEM")

    // ---- Symbol table ----
    struct Symbol {
        OberonTypePtr type;
        llvm::Value*  llvmVal  = nullptr;
        bool          isConst  = false;
        bool          isVar    = false; // VAR param – llvmVal is already a ptr
        llvm::Value*  tagVal   = nullptr; // VAR record param: hidden dynamic type tag
    };
    using Scope = std::unordered_map<std::string, Symbol>;
    std::vector<Scope> scopes_;

    // ---- String literal dedup ----
    std::unordered_map<std::string, llvm::GlobalVariable*> strLiterals_;

    // ---- External / imported functions ----
    std::unordered_map<std::string, llvm::Function*> extFuncs_;

    // ---- Formal parameter tracking ----
    // Per Oberon formal (not per LLVM parameter): whether the actual is passed
    // by address, and which hidden argument (if any) follows it.
    enum class Hidden : uint8_t { None, Len, Tag };
    struct Formal {
        OberonTypePtr type;
        bool          byRef  = false; // VAR, array or record: pass address
        Hidden        hidden = Hidden::None; // Len: open-array i64 length; Tag: VAR record type tag
    };
    std::unordered_map<llvm::Function*, std::vector<Formal>> formals_;
    std::unordered_map<llvm::Function*, OberonTypePtr>       procRetTypes_;

    // ---- Per-function state ----
    llvm::Function*   curFunc_    = nullptr;
    llvm::BasicBlock* exitBlock_  = nullptr; // unified function exit
    llvm::AllocaInst* retAlloca_  = nullptr; // holds return value

    // ----------------------------------------------------------------
    // Type handling
    // ----------------------------------------------------------------
    void          registerBuiltins();
    OberonTypePtr resolveTypeName(const std::string& mod, const std::string& name);
    OberonTypePtr resolveType(const TypeExpr& te);
    llvm::Type*         toLLVM(OberonTypePtr t);
    llvm::IntegerType*  intTy();   // LLVM type of INTEGER/SET values (i32 or i64)
    llvm::ConstantInt*  intConst(int64_t v);   // INTEGER constant, wrapped to intTy()
    llvm::Type*         realTy();  // LLVM type of REAL (double, or float with --int32)
    llvm::PointerType*  ptrTy();   // generic data pointer (i8*)
    std::vector<Formal> makeFormals(const std::vector<ProcParam>& params);
    // Appends LLVM parameter types (including hidden ones) for the formals.
    void                formalLLVMTypes(const std::vector<Formal>& fs,
                                        std::vector<llvm::Type*>& out);
    std::vector<ProcParam> paramsOf(const FormalParameters* fp);
    llvm::FunctionType* toLLVMFunctionType(OberonTypePtr t); // unwrapped fn type for indirect calls

    // Two-pass type declaration processing
    void registerTypeDecls(const std::vector<TypeDecl>& decls); // pass 1: stubs
    void fillTypeDecls   (const std::vector<TypeDecl>& decls); // pass 2: bodies

    // ----------------------------------------------------------------
    // Declaration generation
    // ----------------------------------------------------------------
    void genImports   (const std::vector<ImportEntry>& imports);
    void loadModuleInterface(const std::string& alias, const std::string& modName);
    void genConstDecls(const std::vector<ConstDecl>&   decls, bool global);
    void genVarDecls  (const std::vector<VarDecl>&     decls, bool global);
    void declareProcs (const std::vector<std::shared_ptr<ProcDecl>>& procs);
    void genProc      (const ProcDecl& pd);
    void genDecls     (const DeclarationSequence& ds, bool global);

    // ----------------------------------------------------------------
    // Statement generation (codegen_gen.cpp)
    // ----------------------------------------------------------------
    void genStmts (const StmtSeq& stmts);
    void genStmt  (const Stmt&    s);
    void genAssign(const AssignStmt&    s);
    void genCall  (const ProcCallStmt&  s);
    void genSysCall(const ProcCallStmt& s);   // SYSTEM proper-procedure intrinsics
    void genIf    (const IfStmt&        s);
    void genCase  (const CaseStmt&      s);
    void genWhile (const WhileStmt&     s);
    void genRepeat(const RepeatStmt&    s);
    void genFor   (const ForStmt&       s);

    // ----------------------------------------------------------------
    // Expression generation (codegen_gen.cpp)
    // ----------------------------------------------------------------
    struct AddrResult { llvm::Value* addr; OberonTypePtr type; };

    llvm::Value* genExpr      (const Expr&       e);
    AddrResult   genAddr      (const Designator& d, size_t maxSelectors = SIZE_MAX);
    std::vector<llvm::Value*> genArgs(const std::vector<ExprPtr>* actuals,
                                      llvm::FunctionType* fty,
                                      const std::vector<Formal>* formals);
    void         genArrayOrRecordCopy(llvm::Value* dst, OberonTypePtr dstTy, const Expr& src,
                                      llvm::Value* dstLen = nullptr);
    llvm::Value* genStringOperand(const Expr& e);
    llvm::Value* genLength(const Expr& arrayArg, OberonTypePtr argTy); // i64 element count
    llvm::Value* genCallVal   (const DesignatorExpr& de);   // call as expression
    llvm::Value* genSysCallVal(const DesignatorExpr& de);   // SYSTEM function intrinsics

    // Returns the resolved OberonType if expr is a bare type-name designator
    // (used by SYSTEM.SIZE and SYSTEM.VAL), otherwise nullptr.
    OberonTypePtr tryGetTypeArg(const Expr& e);
    // If de has the call form v(T) with T a record or pointer type, it is a
    // type guard (the parser cannot tell it from a call); returns T.
    OberonTypePtr typeGuardOf(const DesignatorExpr& de);

    // ----------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------
    llvm::GlobalVariable* getOrCreateStrLit(const std::string& content);
    llvm::Value*          strLitPtr(llvm::GlobalVariable* gv); // [N x i8]* → i8*
    int64_t               parseIntLit(const std::string& raw);
    double                parseRealLit(const std::string& raw);
    std::string           parseStrContent(const std::string& raw); // strip delimiters
    int64_t               evalConstInt(const Expr& e);  // evaluate compile-time integer

    OberonTypePtr typeOfDesig(const Designator& d);
    OberonTypePtr exprType(const Expr& e);   // best-effort static type, nullptr if unknown
    bool          isSetExpr(const Expr& e);
    static bool   isCharArray(const OberonTypePtr& t);

    // ---- Run-time type information (record type descriptors) ----
    // A descriptor is a global { i8* base } named "<Module>_<Type>__td".
    // Heap records carry their descriptor in an 8-byte header before the
    // object; VAR record parameters receive it as a hidden argument.
    llvm::StructType* tdStructTy();
    llvm::Value*      tdRef(const OberonTypePtr& recTy);   // i8*, null if no descriptor
    void              emitTypeDescriptor(const OberonTypePtr& recTy);
    llvm::Value*      genRecordTag(const Designator& d, const OberonTypePtr& staticTy);
    llvm::Value*      genIsTest(const Designator& d, const OberonTypePtr& target);
    llvm::Function*   getRuntimeHelper(const std::string& name); // module-private helpers

    void          pushScope() { scopes_.emplace_back(); }
    void          popScope () { scopes_.pop_back(); }
    void          addSym(const std::string& n, Symbol s) { scopes_.back()[n] = std::move(s); }
    Symbol*       lookupSym(const std::string& n);

    llvm::AllocaInst* createEntryAlloca(llvm::Function* fn, llvm::Type* t,
                                         const std::string& name);
    llvm::Value*      coerce(llvm::Value* v, llvm::Type* targetTy);
    bool              blockTerminated();

    [[noreturn]] void error(const std::string& msg) {
        throw std::runtime_error("codegen: " + msg);
    }
};

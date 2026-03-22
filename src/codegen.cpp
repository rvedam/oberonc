#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IR/DataLayout.h>

#include <cassert>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

// -----------------------------------------------------------------------
// Constructor / writeIR
// -----------------------------------------------------------------------
CodeGen::CodeGen(const std::string& moduleName)
    : llvmMod_(std::make_unique<llvm::Module>(moduleName, ctx_))
    , b_(std::make_unique<llvm::IRBuilder<>>(ctx_))
    , modName_(moduleName)
{}

void CodeGen::writeIR(const std::string& path) {
    std::error_code ec;
    llvm::raw_fd_ostream os(path, ec, llvm::sys::fs::OF_None);
    if (ec) throw std::runtime_error("cannot open output: " + ec.message());
    writeIR(os);
}

void CodeGen::writeIR(llvm::raw_ostream& os) {
    std::string err;
    llvm::raw_string_ostream es(err);
    if (llvm::verifyModule(*llvmMod_, &es)) {
        es.flush();
        throw std::runtime_error("LLVM module verification failed:\n" + err);
    }
    llvmMod_->print(os, nullptr);
}

void CodeGen::writeObject(const std::string& path) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    std::string targetTriple = llvm::sys::getDefaultTargetTriple();
    llvmMod_->setTargetTriple(targetTriple);

    std::string err;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTriple, err);
    if (!target)
        throw std::runtime_error("LLVM target lookup failed: " + err);

    llvm::TargetOptions opt;
    auto rm = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
    auto* tm = target->createTargetMachine(targetTriple, "generic", "", opt, rm);
    llvmMod_->setDataLayout(tm->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
    if (ec) throw std::runtime_error("cannot open object output: " + ec.message());

    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, dest, nullptr, llvm::CGFT_ObjectFile))
        throw std::runtime_error("LLVM cannot emit object file for this target");

    pm.run(*llvmMod_);
    dest.flush();
    delete tm;
}

// -----------------------------------------------------------------------
// Symbol-table helpers
// -----------------------------------------------------------------------
CodeGen::Symbol* CodeGen::lookupSym(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return &it->second;
    }
    return nullptr;
}

llvm::AllocaInst* CodeGen::createEntryAlloca(llvm::Function* fn,
                                              llvm::Type*     t,
                                              const std::string& name) {
    llvm::IRBuilder<> tmp(&fn->getEntryBlock(),
                           fn->getEntryBlock().begin());
    return tmp.CreateAlloca(t, nullptr, name);
}

bool CodeGen::blockTerminated() {
    auto* bb = b_->GetInsertBlock();
    return bb && bb->getTerminator() != nullptr;
}

llvm::Value* CodeGen::coerce(llvm::Value* v, llvm::Type* tgt) {
    if (!v || !tgt || v->getType() == tgt) return v;
    // Null pointer to any pointer type
    if (tgt->isPointerTy() && llvm::isa<llvm::ConstantPointerNull>(v))
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(tgt));
    // Pointer → pointer bitcast
    if (v->getType()->isPointerTy() && tgt->isPointerTy())
        return b_->CreateBitCast(v, tgt);
    // Integer widen/narrow
    if (v->getType()->isIntegerTy() && tgt->isIntegerTy())
        return b_->CreateSExtOrTrunc(v, tgt);
    // i1 → i8 (bool stored as char)
    if (v->getType()->isIntegerTy(1) && tgt->isIntegerTy(8))
        return b_->CreateZExt(v, tgt);
    return v;
}

// -----------------------------------------------------------------------
// Type system helpers
// -----------------------------------------------------------------------
void CodeGen::registerBuiltins() {
    auto make = [](TypeKind k, const std::string& n) {
        auto t = std::make_shared<OberonType>();
        t->kind = k; t->name = n;
        return t;
    };
    typeTable_["INTEGER"]  = make(TypeKind::Integer,  "INTEGER");
    typeTable_["REAL"]     = make(TypeKind::Real,     "REAL");
    typeTable_["BOOLEAN"]  = make(TypeKind::Boolean,  "BOOLEAN");
    typeTable_["CHAR"]     = make(TypeKind::Char,     "CHAR");
    typeTable_["BYTE"]     = make(TypeKind::Byte,     "BYTE");
    typeTable_["SET"]      = make(TypeKind::Set,      "SET");
    typeTable_["STRING"]   = make(TypeKind::String,   "STRING");
}

OberonTypePtr CodeGen::resolveTypeName(const std::string& mod,
                                        const std::string& name) {
    // Ignore module qualifier for built-ins and local types in this pass
    (void)mod;
    auto it = typeTable_.find(name);
    if (it != typeTable_.end()) return it->second;
    error("unknown type: " + (mod.empty() ? name : mod + "." + name));
}

OberonTypePtr CodeGen::resolveType(const TypeExpr& te) {
    if (auto* q = dynamic_cast<const QualIdentType*>(&te))
        return resolveTypeName(q->module, q->ident);

    if (auto* a = dynamic_cast<const ArrayTypeExpr*>(&te)) {
        auto t = std::make_shared<OberonType>();
        t->kind = TypeKind::Array;
        t->isOpen = a->lengths.empty();
        if (!t->isOpen) {
            // Use the last dimension as the innermost array, then wrap outward
            t->length = evalConstInt(*a->lengths.back());
        }
        t->elem = resolveType(*a->elemType);
        // For multi-dimensional: ARRAY d0, d1 OF T → ARRAY d0 OF (ARRAY d1 OF T)
        // The first pass above built the innermost ARRAY d_last OF T.
        // Now wrap from second-to-last dimension down to the first.
        if (!t->isOpen && a->lengths.size() > 1) {
            for (int i = static_cast<int>(a->lengths.size()) - 2; i >= 0; --i) {
                auto outer = std::make_shared<OberonType>();
                outer->kind = TypeKind::Array;
                outer->length = evalConstInt(*a->lengths[static_cast<size_t>(i)]);
                outer->elem = t;
                t = outer;
            }
        }
        return t;
    }

    if (auto* r = dynamic_cast<const RecordTypeExpr*>(&te)) {
        auto t = std::make_shared<OberonType>();
        t->kind = TypeKind::Record;
        t->baseName   = r->baseIdent;
        t->baseModule = r->baseModule;
        for (auto& fl : r->fields)
            for (auto& fn : fl.names)
                t->fields.push_back({fn, resolveType(*fl.type)});
        return t;
    }

    if (auto* p = dynamic_cast<const PointerTypeExpr*>(&te)) {
        auto t = std::make_shared<OberonType>();
        t->kind = TypeKind::Pointer;
        // If the base is a QualIdent we may not have defined it yet; record the name
        if (auto* q = dynamic_cast<const QualIdentType*>(p->base.get())) {
            t->ptrBaseName = q->ident;
            // Try to resolve now; if not found, we'll resolve in fillTypeDecls
            auto it = typeTable_.find(q->ident);
            if (it != typeTable_.end()) t->ptrBase = it->second;
        } else {
            t->ptrBase = resolveType(*p->base);
        }
        return t;
    }

    if (auto* pt = dynamic_cast<const ProcedureTypeExpr*>(&te)) {
        auto t = std::make_shared<OberonType>();
        t->kind = TypeKind::Procedure;
        if (pt->params) {
            for (auto& sec : pt->params->sections)
                for (auto& pn : sec.names) {
                    ProcParam pp;
                    pp.isVar = sec.isVar;
                    pp.name  = pn;
                    pp.type  = resolveType(*sec.formalType);
                    t->params.push_back(std::move(pp));
                }
            if (!pt->params->retIdent.empty())
                t->retType = resolveTypeName(pt->params->retModule,
                                              pt->params->retIdent);
        }
        return t;
    }

    error("unsupported type expression");
}

llvm::Type* CodeGen::toLLVM(OberonTypePtr t) {
    if (!t) return llvm::Type::getVoidTy(ctx_);
    auto it = llvmTypeCache_.find(t.get());
    if (it != llvmTypeCache_.end()) return it->second;

    llvm::Type* result = nullptr;
    switch (t->kind) {
    case TypeKind::Integer:
    case TypeKind::Set:
        result = llvm::Type::getInt64Ty(ctx_); break;
    case TypeKind::Real:
        result = llvm::Type::getDoubleTy(ctx_); break;
    case TypeKind::Boolean:
        result = llvm::Type::getInt1Ty(ctx_); break;
    case TypeKind::Char:
    case TypeKind::Byte:
        result = llvm::Type::getInt8Ty(ctx_); break;
    case TypeKind::Nil:
    case TypeKind::String:
        result = llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0); break;
    case TypeKind::Array:
        if (t->isOpen)
            // Open arrays passed as raw element pointer
            result = llvm::PointerType::get(toLLVM(t->elem), 0);
        else
            result = llvm::ArrayType::get(toLLVM(t->elem),
                                           static_cast<uint64_t>(t->length));
        break;
    case TypeKind::Record: {
        // Check if we already have a named struct in the module
        auto* named = llvm::StructType::getTypeByName(ctx_, modName_ + "_" + t->name);
        if (named) { result = named; break; }
        // Create anonymous struct on the fly
        std::vector<llvm::Type*> fieldTypes;
        for (auto& f : t->fields)
            fieldTypes.push_back(toLLVM(f.type));
        result = llvm::StructType::get(ctx_, fieldTypes); break;
    }
    case TypeKind::Pointer:
        if (t->ptrBase)
            result = llvm::PointerType::get(toLLVM(t->ptrBase), 0);
        else
            result = llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0);
        break;
    case TypeKind::Procedure: {
        std::vector<llvm::Type*> pts;
        for (auto& p : t->params) {
            llvm::Type* pt = toLLVM(p.type);
            if (p.isVar) pt = llvm::PointerType::get(pt, 0);
            pts.push_back(pt);
        }
        auto* rty = t->retType ? toLLVM(t->retType) : llvm::Type::getVoidTy(ctx_);
        result = llvm::PointerType::get(llvm::FunctionType::get(rty, pts, false), 0);
        break;
    }
    }
    if (result) llvmTypeCache_[t.get()] = result;
    return result;
}

// -----------------------------------------------------------------------
// Pass 1: register forward-declared LLVM struct types for all RECORDs
// -----------------------------------------------------------------------
void CodeGen::registerTypeDecls(const std::vector<TypeDecl>& decls) {
    for (auto& td : decls) {
        auto* re = dynamic_cast<const RecordTypeExpr*>(td.type.get());
        if (!re) continue;
        std::string llvmName = modName_ + "_" + td.name;
        if (!llvm::StructType::getTypeByName(ctx_, llvmName)) {
            auto* st = llvm::StructType::create(ctx_, llvmName);
            // Build the OberonType stub so forward pointers can reference it
            auto ot = std::make_shared<OberonType>();
            ot->kind = TypeKind::Record;
            ot->name = td.name;
            typeTable_[td.name] = ot;
            // Cache the LLVM type immediately
            llvmTypeCache_[ot.get()] = st;
        }
    }
}

// -----------------------------------------------------------------------
// Pass 2: fill in struct bodies and resolve pointer forward-references
// -----------------------------------------------------------------------
void CodeGen::fillTypeDecls(const std::vector<TypeDecl>& decls) {
    for (auto& td : decls) {
        OberonTypePtr ot = resolveType(*td.type);
        ot->name = td.name;

        // If we already registered a stub, fill the LLVM struct body
        if (ot->kind == TypeKind::Record) {
            auto* st = llvm::dyn_cast_or_null<llvm::StructType>(
                llvm::StructType::getTypeByName(ctx_, modName_ + "_" + td.name));
            if (st && st->isOpaque()) {
                std::vector<llvm::Type*> fts;
                for (auto& f : ot->fields)
                    fts.push_back(toLLVM(f.type));
                st->setBody(fts);
            }
            // Update the cached Oberon type entry with the filled-in one
            auto existing = typeTable_.find(td.name);
            if (existing != typeTable_.end()) {
                // Merge fields into the existing stub
                existing->second->fields     = ot->fields;
                existing->second->baseName   = ot->baseName;
                existing->second->baseModule = ot->baseModule;
            } else {
                typeTable_[td.name] = ot;
            }
        } else {
            typeTable_[td.name] = ot;
        }
    }

    // Resolve pointer forward-references
    for (auto& [name, ot] : typeTable_) {
        if (ot->kind == TypeKind::Pointer && !ot->ptrBase && !ot->ptrBaseName.empty()) {
            auto it = typeTable_.find(ot->ptrBaseName);
            if (it != typeTable_.end()) {
                ot->ptrBase = it->second;
                // Update LLVM type cache for this pointer
                llvmTypeCache_[ot.get()] =
                    llvm::PointerType::get(toLLVM(ot->ptrBase), 0);
            }
        }
    }
}

// -----------------------------------------------------------------------
// Import declarations
// Declares external C functions matching the runtime library.
// -----------------------------------------------------------------------
void CodeGen::genImports(const std::vector<ImportEntry>& imports) {
    auto* voidTy = llvm::Type::getVoidTy(ctx_);
    auto* i8Ptr  = llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0);
    auto* i64    = llvm::Type::getInt64Ty(ctx_);
    auto* i64Ptr = llvm::PointerType::get(i64, 0);
    auto* dbl    = llvm::Type::getDoubleTy(ctx_);

    // Always declare Oberon_NEW (used by the NEW() built-in)
    {
        auto* ft = llvm::FunctionType::get(i8Ptr, {i64}, false);
        extFuncs_["Oberon_NEW"] =
            llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                    "Oberon_NEW", llvmMod_.get());
    }

    for (auto& imp : imports) {
        std::string modAlias = imp.alias.empty() ? imp.name : imp.alias;
        importedModules_.insert(modAlias);
        std::string mod = imp.name;

        if (mod == "Out") {
            auto decl = [&](const char* fn, llvm::FunctionType* ft) {
                auto* f = llvm::Function::Create(
                    ft, llvm::Function::ExternalLinkage, fn, llvmMod_.get());
                extFuncs_[std::string(fn)] = f;
            };
            decl("Out_String",
                 llvm::FunctionType::get(voidTy, {i8Ptr}, false));
            decl("Out_Ln",
                 llvm::FunctionType::get(voidTy, {}, false));
            decl("Out_Int",
                 llvm::FunctionType::get(voidTy, {i64, i64}, false));
            decl("Out_Real",
                 llvm::FunctionType::get(voidTy, {dbl, i64}, false));
            decl("Out_Char",
                 llvm::FunctionType::get(voidTy, {llvm::Type::getInt8Ty(ctx_)}, false));
        } else if (mod == "In") {
            auto decl = [&](const char* fn, llvm::FunctionType* ft) {
                extFuncs_[std::string(fn)] =
                    llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                            fn, llvmMod_.get());
            };
            decl("In_Int",  llvm::FunctionType::get(voidTy, {i64Ptr}, false));
            decl("In_Real", llvm::FunctionType::get(voidTy,
                {llvm::PointerType::get(dbl, 0)}, false));
            decl("In_Char", llvm::FunctionType::get(voidTy,
                {llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0)}, false));
        }
        // Other (user-defined) modules: parse source and declare exported procs
        else {
            loadModuleInterface(modAlias, mod);
        }
    }
}

// -----------------------------------------------------------------------
// Load an imported user-defined module's interface by parsing its source
// -----------------------------------------------------------------------
void CodeGen::loadModuleInterface(const std::string& alias,
                                   const std::string& modName) {
    // Search for ModuleName.Mod in the module search paths
    std::string source;
    bool found = false;
    for (auto& dir : modulePaths_) {
        std::string path = dir + "/" + modName + ".Mod";
        std::ifstream f(path);
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            source = ss.str();
            found = true;
            break;
        }
    }
    if (!found) return; // silently skip; errors will surface at link time

    loadedUserModules_.push_back({alias, modName});

    // Parse the module to extract exported procedure signatures
    Module importedMod;
    try {
        Lexer  lex(source, modName + ".Mod");
        Parser parser(lex);
        importedMod = parser.parseModule();
    } catch (...) {
        return; // parse failure → skip
    }

    // Declare each exported procedure as an external LLVM function
    for (auto& pd : importedMod.decls.procs) {
        if (!pd->exported) continue;

        std::string fname = modName + "_" + pd->name;
        if (llvmMod_->getFunction(fname)) continue; // already declared

        std::vector<llvm::Type*> paramTys;
        if (pd->params) {
            for (auto& sec : pd->params->sections) {
                // Map Oberon types to LLVM types using the type name
                llvm::Type* lt = nullptr;
                if (auto* q = dynamic_cast<const QualIdentType*>(sec.formalType.get())) {
                    auto it = typeTable_.find(q->ident);
                    if (it != typeTable_.end())
                        lt = toLLVM(it->second);
                }
                if (!lt) lt = llvm::Type::getInt64Ty(ctx_); // default to i64
                if (sec.isVar) lt = llvm::PointerType::get(lt, 0);
                for (size_t i = 0; i < sec.names.size(); ++i)
                    paramTys.push_back(lt);
            }
        }

        llvm::Type* retTy = llvm::Type::getVoidTy(ctx_);
        if (pd->params && !pd->params->retIdent.empty()) {
            auto it = typeTable_.find(pd->params->retIdent);
            if (it != typeTable_.end())
                retTy = toLLVM(it->second);
        }

        auto* ft = llvm::FunctionType::get(retTy, paramTys, false);
        auto* fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                           fname, llvmMod_.get());
        extFuncs_[alias + "_" + pd->name] = fn;
    }
}

// -----------------------------------------------------------------------
// Constant declarations
// -----------------------------------------------------------------------
void CodeGen::genConstDecls(const std::vector<ConstDecl>& decls, bool global) {
    for (auto& cd : decls) {
        Symbol sym;
        sym.isConst = true;
        if (auto* s = dynamic_cast<const StringLitExpr*>(cd.value.get())) {
            std::string content = parseStrContent(s->raw);
            auto* gv = getOrCreateStrLit(content);
            auto t = std::make_shared<OberonType>();
            t->kind = TypeKind::String;
            t->name = "STRING";
            sym.type    = t;
            sym.llvmVal = gv;
        } else if (auto* i = dynamic_cast<const IntLitExpr*>(cd.value.get())) {
            sym.type    = typeTable_["INTEGER"];
            sym.llvmVal = llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(ctx_), parseIntLit(i->raw));
        } else if (auto* r = dynamic_cast<const RealLitExpr*>(cd.value.get())) {
            sym.type    = typeTable_["REAL"];
            sym.llvmVal = llvm::ConstantFP::get(
                llvm::Type::getDoubleTy(ctx_), parseRealLit(r->raw));
        } else if (dynamic_cast<const TrueExpr*>(cd.value.get())) {
            sym.type    = typeTable_["BOOLEAN"];
            sym.llvmVal = llvm::ConstantInt::getTrue(ctx_);
        } else if (dynamic_cast<const FalseExpr*>(cd.value.get())) {
            sym.type    = typeTable_["BOOLEAN"];
            sym.llvmVal = llvm::ConstantInt::getFalse(ctx_);
        } else {
            // General expression: generate it in current function context.
            // If global (module level), we can't do that yet; skip for now.
            if (!global) {
                sym.type    = typeTable_["INTEGER"]; // best guess
                sym.llvmVal = genExpr(*cd.value);
            }
        }
        if (sym.llvmVal) addSym(cd.name, std::move(sym));
    }
}

// -----------------------------------------------------------------------
// Variable declarations
// -----------------------------------------------------------------------
void CodeGen::genVarDecls(const std::vector<VarDecl>& decls, bool global) {
    for (auto& vd : decls) {
        OberonTypePtr ot = resolveType(*vd.type);
        llvm::Type*   lt = toLLVM(ot);

        for (auto& name : vd.names) {
            Symbol sym;
            sym.type    = ot;
            sym.isConst = false;

            if (global) {
                // Module-level: create a global variable
                std::string gname = modName_ + "_" + name;
                llvm::Constant* init = nullptr;
                if (lt->isIntegerTy())
                    init = llvm::ConstantInt::get(lt, 0);
                else if (lt->isDoubleTy())
                    init = llvm::ConstantFP::get(lt, 0.0);
                else if (lt->isPointerTy())
                    init = llvm::ConstantPointerNull::get(
                        llvm::cast<llvm::PointerType>(lt));
                else
                    init = llvm::ConstantAggregateZero::get(lt);

                auto* gv = new llvm::GlobalVariable(
                    *llvmMod_, lt, false,
                    llvm::GlobalValue::InternalLinkage, init, gname);
                sym.llvmVal = gv;
            } else {
                // Procedure-local: create an alloca in the function entry block
                sym.llvmVal = createEntryAlloca(curFunc_, lt, name);
                // Zero-initialize
                b_->CreateStore(llvm::Constant::getNullValue(lt), sym.llvmVal);
            }
            addSym(name, std::move(sym));
        }
    }
}

// -----------------------------------------------------------------------
// Procedure declarations (forward pass – declare LLVM functions only)
// -----------------------------------------------------------------------
void CodeGen::declareProcs(const std::vector<std::shared_ptr<ProcDecl>>& procs) {
    for (auto& pd : procs) {
        std::string fname = modName_ + "_" + pd->name;

        // Build LLVM parameter types
        std::vector<llvm::Type*> paramTys;
        if (pd->params) {
            for (auto& sec : pd->params->sections) {
                OberonTypePtr ot = resolveType(*sec.formalType);
                llvm::Type*   lt = toLLVM(ot);
                if (sec.isVar || ot->kind == TypeKind::Array)
                    lt = llvm::PointerType::get(
                             ot->isOpen ? toLLVM(ot->elem) : lt, 0);
                for (size_t i = 0; i < sec.names.size(); ++i)
                    paramTys.push_back(lt);
            }
        }
        llvm::Type* retTy = llvm::Type::getVoidTy(ctx_);
        if (pd->params && !pd->params->retIdent.empty())
            retTy = toLLVM(resolveTypeName(pd->params->retModule,
                                            pd->params->retIdent));

        auto* ft = llvm::FunctionType::get(retTy, paramTys, false);
        llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                fname, llvmMod_.get());
    }
}

// -----------------------------------------------------------------------
// Full procedure body generation
// -----------------------------------------------------------------------
void CodeGen::genProc(const ProcDecl& pd) {
    std::string fname = modName_ + "_" + pd.name;
    auto* fn = llvmMod_->getFunction(fname);
    if (!fn) error("procedure not forward-declared: " + pd.name);

    curFunc_   = fn;
    auto* entry = llvm::BasicBlock::Create(ctx_, "entry", fn);
    exitBlock_  = llvm::BasicBlock::Create(ctx_, "exit",  fn);
    b_->SetInsertPoint(entry);

    pushScope();

    // Create return alloca if function returns a value
    retAlloca_ = nullptr;
    if (!fn->getReturnType()->isVoidTy())
        retAlloca_ = createEntryAlloca(fn, fn->getReturnType(), "retval");

    // Bind parameters
    if (pd.params) {
        auto argIt = fn->arg_begin();
        for (auto& sec : pd.params->sections) {
            OberonTypePtr ot = resolveType(*sec.formalType);
            for (auto& pname : sec.names) {
                Symbol sym;
                sym.type = ot;
                if (sec.isVar || ot->kind == TypeKind::Array) {
                    // VAR or open-array param: argument IS the pointer
                    sym.llvmVal = &*argIt;
                    sym.isVar   = true;
                } else {
                    // Value param: create alloca, store argument
                    llvm::Type* lt = toLLVM(ot);
                    sym.llvmVal = createEntryAlloca(fn, lt, pname);
                    b_->CreateStore(&*argIt, sym.llvmVal);
                }
                argIt->setName(pname);
                addSym(pname, std::move(sym));
                ++argIt;
            }
        }
    }

    // Nested declarations (call helpers directly — ProcDecl holds unique_ptrs, no copy)
    registerTypeDecls(pd.types);
    fillTypeDecls(pd.types);
    genConstDecls(pd.consts, /*global=*/false);
    genVarDecls  (pd.vars,   /*global=*/false);
    declareProcs (pd.procs);
    for (auto& nestedProc : pd.procs)
        genProc(*nestedProc);

    // Body
    genStmts(pd.body);

    // Explicit RETURN expression (outside of statement list)
    if (pd.returnExpr && !blockTerminated()) {
        auto* v = genExpr(*pd.returnExpr);
        if (retAlloca_) b_->CreateStore(coerce(v, fn->getReturnType()), retAlloca_);
        b_->CreateBr(exitBlock_);
    } else if (!blockTerminated()) {
        b_->CreateBr(exitBlock_);
    }

    // Exit block: return value or void
    b_->SetInsertPoint(exitBlock_);
    if (retAlloca_)
        b_->CreateRet(b_->CreateLoad(fn->getReturnType(), retAlloca_));
    else
        b_->CreateRetVoid();

    popScope();
    curFunc_   = nullptr;
    exitBlock_  = nullptr;
    retAlloca_ = nullptr;
}

// -----------------------------------------------------------------------
// DeclarationSequence  (delegates to const/var/type/proc generators)
// -----------------------------------------------------------------------
void CodeGen::genDecls(const DeclarationSequence& ds, bool global) {
    registerTypeDecls(ds.types);
    fillTypeDecls(ds.types);
    genConstDecls(ds.consts, global);
    genVarDecls  (ds.vars,   global);
    declareProcs (ds.procs);
    for (auto& pd : ds.procs)
        genProc(*pd);
}

// -----------------------------------------------------------------------
// Top-level entry point
// -----------------------------------------------------------------------
void CodeGen::generate(const Module& mod) {
    registerBuiltins();
    pushScope(); // module scope

    genImports(mod.imports);
    genDecls(mod.decls, /*global=*/true);

    // Generate void oberon_main() from the module body, OR
    // void ModuleName_init() when compiled as an imported dependency.
    std::string mainFuncName = moduleInitOnly_
        ? (modName_ + "_init")
        : "oberon_main";
    auto* mainTy = llvm::FunctionType::get(
        llvm::Type::getVoidTy(ctx_), {}, false);
    auto* mainFn = llvm::Function::Create(
        mainTy, llvm::Function::ExternalLinkage, mainFuncName, llvmMod_.get());

    curFunc_   = mainFn;
    auto* entry = llvm::BasicBlock::Create(ctx_, "entry", mainFn);
    exitBlock_  = llvm::BasicBlock::Create(ctx_, "exit",  mainFn);
    retAlloca_ = nullptr;
    b_->SetInsertPoint(entry);

    pushScope();
    genStmts(mod.body);
    if (!blockTerminated()) b_->CreateBr(exitBlock_);

    b_->SetInsertPoint(exitBlock_);
    b_->CreateRetVoid();
    popScope();

    curFunc_   = nullptr;
    exitBlock_  = nullptr;
    popScope(); // module scope
}

// -----------------------------------------------------------------------
// String literal helpers
// -----------------------------------------------------------------------
std::string CodeGen::parseStrContent(const std::string& raw) {
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
        return raw.substr(1, raw.size() - 2);
    // Hex char constant: digit{hexDigit}X  e.g. "0DX" → "\r"
    if (!raw.empty() && raw.back() == 'X') {
        std::string hex = raw.substr(0, raw.size() - 1);
        long v = std::strtol(hex.c_str(), nullptr, 16);
        return std::string(1, static_cast<char>(v));
    }
    return raw;
}

llvm::GlobalVariable* CodeGen::getOrCreateStrLit(const std::string& content) {
    auto it = strLiterals_.find(content);
    if (it != strLiterals_.end()) return it->second;

    // Create [N+1 x i8] null-terminated constant
    auto* arrTy = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx_),
                                        content.size() + 1);
    std::vector<llvm::Constant*> chars;
    for (unsigned char c : content)
        chars.push_back(llvm::ConstantInt::get(
            llvm::Type::getInt8Ty(ctx_), c));
    chars.push_back(llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx_), 0));
    auto* init = llvm::ConstantArray::get(arrTy, chars);
    auto* gv   = new llvm::GlobalVariable(
        *llvmMod_, arrTy, true,
        llvm::GlobalValue::PrivateLinkage, init, ".str");
    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    strLiterals_[content] = gv;
    return gv;
}

llvm::Value* CodeGen::strLitPtr(llvm::GlobalVariable* gv) {
    auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0);
    return b_->CreateInBoundsGEP(gv->getValueType(), gv, {zero, zero}, "str");
}

// -----------------------------------------------------------------------
// Literal parsing helpers
// -----------------------------------------------------------------------
int64_t CodeGen::parseIntLit(const std::string& raw) {
    if (!raw.empty() && raw.back() == 'H') {
        // Hex integer
        return static_cast<int64_t>(
            std::strtoull(raw.substr(0, raw.size() - 1).c_str(), nullptr, 16));
    }
    return static_cast<int64_t>(std::strtoll(raw.c_str(), nullptr, 10));
}

double CodeGen::parseRealLit(const std::string& raw) {
    return std::strtod(raw.c_str(), nullptr);
}

// Evaluate a compile-time constant integer expression.
// Handles integer literals and references to named integer constants.
int64_t CodeGen::evalConstInt(const Expr& e) {
    if (auto* il = dynamic_cast<const IntLitExpr*>(&e))
        return parseIntLit(il->raw);

    if (auto* ue = dynamic_cast<const UnaryExpr*>(&e)) {
        if (ue->op == UnaryOp::Minus) return -evalConstInt(*ue->operand);
        if (ue->op == UnaryOp::Plus)  return  evalConstInt(*ue->operand);
    }

    if (auto* de = dynamic_cast<const DesignatorExpr*>(&e)) {
        if (!de->args && de->desig->selectors.empty()) {
            auto* sym = lookupSym(de->desig->ident);
            if (sym && sym->isConst && sym->llvmVal) {
                if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(sym->llvmVal))
                    return static_cast<int64_t>(ci->getSExtValue());
            }
        }
    }

    error("array length is not a constant integer expression");
}

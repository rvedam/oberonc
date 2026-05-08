#include "codegen.hpp"

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Intrinsics.h>

// -----------------------------------------------------------------------
// Type-of helper  (used to know which LLVM ops to emit)
// -----------------------------------------------------------------------
OberonTypePtr CodeGen::typeOfDesig(const Designator& d) {
    OberonTypePtr ty;

    if (!d.module.empty() && !importedModules_.contains(d.module)) {
        // module is actually a record variable name
        auto* sym = lookupSym(d.module);
        if (!sym) error("unknown symbol: " + d.module);
        ty = sym->type;
        if (ty->kind == TypeKind::Pointer) ty = ty->ptrBase;
        int idx = ty->fieldIndex(d.ident);
        if (idx < 0) error("field not found: " + d.ident);
        ty = ty->fields[idx].type;
    } else {
        auto* sym = lookupSym(d.ident);
        if (!sym) error("unknown symbol: " + d.ident);
        ty = sym->type;
    }

    for (auto& sel : d.selectors) {
        if (auto* fs = std::get_if<FieldSelector>(&sel)) {
            if (ty->kind == TypeKind::Pointer) ty = ty->ptrBase;
            int idx = ty->fieldIndex(fs->ident);
            if (idx >= 0) ty = ty->fields[idx].type;
        } else if (std::get_if<IndexSelector>(&sel)) {
            if (ty->kind == TypeKind::Array) ty = ty->elem;
        } else if (std::get_if<DerefSelector>(&sel)) {
            if (ty->kind == TypeKind::Pointer) ty = ty->ptrBase;
        }
    }
    return ty;
}

// -----------------------------------------------------------------------
// Address generator  – returns a pointer to the location + its type
// -----------------------------------------------------------------------
CodeGen::AddrResult CodeGen::genAddr(const Designator& d) {
    llvm::Value*  base = nullptr;
    OberonTypePtr ty;

    if (!d.module.empty() && !importedModules_.contains(d.module)) {
        // r.x  where r is a local/global record variable
        auto* sym = lookupSym(d.module);
        if (!sym) error("unknown symbol: " + d.module);
        base = sym->llvmVal;
        ty   = sym->type;

        // If sym is a pointer type, dereference implicitly (e.g. r: POINTER TO REC)
        if (ty->kind == TypeKind::Pointer) {
            base = b_->CreateLoad(toLLVM(ty), base, "ptr");
            ty   = ty->ptrBase;
        }
        // Now access field d.ident
        if (ty->kind != TypeKind::Record)
            error("expected record for field access ." + d.ident);
        int idx = ty->fieldIndex(d.ident);
        if (idx < 0) error("field not found: " + d.ident);
        base = b_->CreateStructGEP(toLLVM(ty), base, idx, d.ident);
        ty   = ty->fields[idx].type;
    } else {
        // If d.module names an imported module, the exported var was registered
        // in the symbol table as "module_ident".
        std::string key = (!d.module.empty() && importedModules_.contains(d.module))
                          ? (d.module + "_" + d.ident)
                          : d.ident;
        auto* sym = lookupSym(key);
        if (!sym) {
            // Try bare ident as fallback (e.g., unqualified local var)
            if (key != d.ident) sym = lookupSym(d.ident);
        }
        if (!sym) error("unknown symbol: " + d.ident);
        base = sym->llvmVal;
        ty   = sym->type;
        // VAR param: llvmVal is already the address of the value
    }

    // Apply selectors
    for (auto& sel : d.selectors) {
        if (auto* fs = std::get_if<FieldSelector>(&sel)) {
            // Field access: dereference if pointer, then GEP
            if (ty->kind == TypeKind::Pointer) {
                base = b_->CreateLoad(toLLVM(ty), base, "ptr");
                ty   = ty->ptrBase;
            }
            if (ty->kind != TypeKind::Record)
                error("field sel on non-record");
            int idx = ty->fieldIndex(fs->ident);
            if (idx < 0) error("field not found: " + fs->ident);
            base = b_->CreateStructGEP(toLLVM(ty), base, idx, fs->ident);
            ty   = ty->fields[idx].type;

        } else if (auto* is = std::get_if<IndexSelector>(&sel)) {
            auto* idx = genExpr(*is->index);
            if (ty->kind != TypeKind::Array)
                error("index sel on non-array");
            if (ty->isOpen) {
                // Open array: base is already T*  →  GEP element pointer
                base = b_->CreateGEP(toLLVM(ty->elem), base, idx, "elem");
            } else {
                // Fixed array: base is [N x T]*  →  GEP {0, idx}
                auto* zero = llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(ctx_), 0);
                base = b_->CreateGEP(toLLVM(ty), base, {zero, idx}, "elem");
            }
            ty = ty->elem;

        } else if (std::get_if<DerefSelector>(&sel)) {
            if (ty->kind != TypeKind::Pointer)
                error("dereference on non-pointer");
            base = b_->CreateLoad(toLLVM(ty), base, "deref");
            ty   = ty->ptrBase;
        }
        // TypeGuardSelector: ignore (semantic phase resolves this)
    }

    return {base, ty};
}

// -----------------------------------------------------------------------
// Expression generator
// -----------------------------------------------------------------------
llvm::Value* CodeGen::genExpr(const Expr& e) {
    // ---- Literals ----
    if (auto* il = dynamic_cast<const IntLitExpr*>(&e))
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_),
                                       parseIntLit(il->raw));

    if (auto* rl = dynamic_cast<const RealLitExpr*>(&e))
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx_),
                                      parseRealLit(rl->raw));

    if (auto* sl = dynamic_cast<const StringLitExpr*>(&e)) {
        std::string content = parseStrContent(sl->raw);
        // Hex char constant (e.g. 0X, 0DX): always a CHAR value → return i8
        bool isHexChar = !sl->raw.empty() && sl->raw.back() == 'X';
        if (isHexChar) {
            unsigned char cv = content.empty() ? '\0' : static_cast<unsigned char>(content[0]);
            return llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx_), cv);
        }
        auto* gv = getOrCreateStrLit(content);
        return strLitPtr(gv);
    }

    if (dynamic_cast<const NilExpr*>(&e))
        return llvm::ConstantPointerNull::get(
            llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0));

    if (dynamic_cast<const TrueExpr*>(&e))
        return llvm::ConstantInt::getTrue(ctx_);

    if (dynamic_cast<const FalseExpr*>(&e))
        return llvm::ConstantInt::getFalse(ctx_);

    // ---- Set literal ----
    if (auto* se = dynamic_cast<const SetExpr*>(&e)) {
        auto* i64 = llvm::Type::getInt64Ty(ctx_);
        llvm::Value* result = llvm::ConstantInt::get(i64, 0);
        auto* one = llvm::ConstantInt::get(i64, 1);
        for (auto& elem : se->elements) {
            llvm::Value* lo = genExpr(*elem.lo);
            if (elem.hi) {
                // Range: try to fold if constants, otherwise loop
                auto* loC = llvm::dyn_cast<llvm::ConstantInt>(lo);
                auto* hiC = llvm::dyn_cast<llvm::ConstantInt>(genExpr(*elem.hi));
                auto* hi = genExpr(*elem.hi);
                if (loC && llvm::isa<llvm::ConstantInt>(hi)) {
                    // Constant fold
                    int64_t mask = 0;
                    int64_t hv = llvm::cast<llvm::ConstantInt>(hi)->getSExtValue();
                    for (int64_t v = loC->getSExtValue(); v <= hv; ++v)
                        mask |= (int64_t(1) << v);
                    result = b_->CreateOr(result, llvm::ConstantInt::get(i64, mask));
                } else {
                    // Runtime: {lo..hi} = (-1 << lo) & (-1 >>> (63 - hi))
                    auto* allOnes = llvm::ConstantInt::get(i64, uint64_t(-1));
                    auto* c63     = llvm::ConstantInt::get(i64, 63);
                    auto* loMask  = b_->CreateShl (allOnes, coerce(lo, i64));
                    auto* hiMask  = b_->CreateLShr(allOnes,
                                        b_->CreateSub(c63, coerce(hi, i64)));
                    result = b_->CreateOr(result, b_->CreateAnd(loMask, hiMask));
                }
            } else {
                auto* bit = b_->CreateShl(one, lo);
                result = b_->CreateOr(result, bit);
            }
        }
        return result;
    }

    // ---- Unary ----
    if (auto* ue = dynamic_cast<const UnaryExpr*>(&e)) {
        auto* v = genExpr(*ue->operand);
        switch (ue->op) {
        case UnaryOp::Minus:
            if (v->getType()->isDoubleTy()) return b_->CreateFNeg(v);
            return b_->CreateNeg(v);
        case UnaryOp::Plus:  return v;
        case UnaryOp::Not:   return b_->CreateNot(v);
        }
    }

    // ---- Binary ----
    if (auto* be = dynamic_cast<const BinaryExpr*>(&e)) {
        auto* lhs = genExpr(*be->left);
        auto* rhs = genExpr(*be->right);
        bool fp = lhs->getType()->isDoubleTy() || rhs->getType()->isDoubleTy();

        // Determine if operands are SET type (vs arithmetic INTEGER/BYTE).
        // In Oberon, /  on integers is illegal — it is always SET symmetric-difference.
        // For +, -, *, check if the left operand was declared as SET.
        auto lhsOberonKind = [&]() -> TypeKind {
            if (auto* de = dynamic_cast<const DesignatorExpr*>(be->left.get())) {
                if (!de->args && de->desig->selectors.empty() && de->desig->module.empty()) {
                    auto* sym = lookupSym(de->desig->ident);
                    if (sym && sym->type) return sym->type->kind;
                }
            }
            return TypeKind::Integer;
        };

        // Normalise integer operand widths: BYTE (i8) widens to the other side,
        // or to i64 if both are narrow; this follows Oberon's implicit widening rules.
        auto normaliseInts = [&](llvm::Value*& l, llvm::Value*& r) {
            if (!l->getType()->isIntegerTy() || !r->getType()->isIntegerTy()) return;
            if (l->getType() == r->getType()) return;
            // Widen the narrower side to the wider, with a floor of i64
            auto* i64 = llvm::Type::getInt64Ty(ctx_);
            unsigned lw = l->getType()->getIntegerBitWidth();
            unsigned rw = r->getType()->getIntegerBitWidth();
            unsigned tgt = std::max({lw, rw, 64u});
            auto* tgtTy  = llvm::Type::getIntNTy(ctx_, tgt);
            if (lw < tgt) l = b_->CreateSExtOrTrunc(l, tgtTy);
            if (rw < tgt) r = b_->CreateSExtOrTrunc(r, tgtTy);
            (void)i64;
        };

        // Reconcile for comparisons: pointer mismatches → i8*; int widths → widen.
        auto reconcile = [&]() -> std::pair<llvm::Value*,llvm::Value*> {
            llvm::Value *l = lhs, *r = rhs;
            if (l->getType() == r->getType()) return {l, r};
            auto* i8p = llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0);
            if (l->getType()->isPointerTy() && r->getType()->isPointerTy())
                return {b_->CreateBitCast(l,i8p), b_->CreateBitCast(r,i8p)};
            if (l->getType()->isIntegerTy() && r->getType()->isIntegerTy()) {
                normaliseInts(l, r);
                return {l, r};
            }
            return {l, r};
        };

        switch (be->op) {
        case BinaryOp::Add: {
            if (fp) return b_->CreateFAdd(lhs, rhs);
            if (lhsOberonKind() == TypeKind::Set) {
                // SET union = OR
                auto [l,r] = reconcile(); normaliseInts(l,r);
                return b_->CreateOr(l, r);
            }
            auto [l,r] = reconcile(); normaliseInts(l,r);
            return b_->CreateAdd(l, r);
        }
        case BinaryOp::Sub: {
            if (fp) return b_->CreateFSub(lhs, rhs);
            if (lhsOberonKind() == TypeKind::Set) {
                // SET difference = AND NOT (a & ~b)
                auto [l,r] = reconcile(); normaliseInts(l,r);
                return b_->CreateAnd(l, b_->CreateNot(r));
            }
            auto [l,r] = reconcile(); normaliseInts(l,r);
            return b_->CreateSub(l, r);
        }
        case BinaryOp::Mul: {
            if (fp) return b_->CreateFMul(lhs, rhs);
            if (lhsOberonKind() == TypeKind::Set) {
                // SET intersection = AND
                auto [l,r] = reconcile(); normaliseInts(l,r);
                return b_->CreateAnd(l, r);
            }
            auto [l,r] = reconcile(); normaliseInts(l,r);
            return b_->CreateMul(l, r);
        }
        case BinaryOp::Div: {
            if (fp) return b_->CreateFDiv(lhs, rhs);
            // Non-fp /  is always SET symmetric difference (XOR); / on INTEGER is invalid Oberon
            auto [l,r] = reconcile(); normaliseInts(l,r);
            return b_->CreateXor(l, r);
        }
        case BinaryOp::IDiv: { auto [l,r] = reconcile(); normaliseInts(l,r); return b_->CreateSDiv(l,r); }
        case BinaryOp::IMod: { auto [l,r] = reconcile(); normaliseInts(l,r); return b_->CreateSRem(l,r); }
        case BinaryOp::And:  { auto [l,r] = reconcile(); normaliseInts(l,r); return b_->CreateAnd(l,r); }
        case BinaryOp::Or:   { auto [l,r] = reconcile(); normaliseInts(l,r); return b_->CreateOr (l,r); }
        case BinaryOp::Eq: {
            auto [l,r] = reconcile();
            return fp ? b_->CreateFCmpOEQ(l,r) : b_->CreateICmpEQ (l,r);
        }
        case BinaryOp::Neq: {
            auto [l,r] = reconcile();
            return fp ? b_->CreateFCmpONE(l,r) : b_->CreateICmpNE (l,r);
        }
        case BinaryOp::Lt: {
            auto [l,r] = reconcile();
            return fp ? b_->CreateFCmpOLT(l,r) : b_->CreateICmpSLT(l,r);
        }
        case BinaryOp::Leq: {
            auto [l,r] = reconcile();
            return fp ? b_->CreateFCmpOLE(l,r) : b_->CreateICmpSLE(l,r);
        }
        case BinaryOp::Gt: {
            auto [l,r] = reconcile();
            return fp ? b_->CreateFCmpOGT(l,r) : b_->CreateICmpSGT(l,r);
        }
        case BinaryOp::Geq: {
            auto [l,r] = reconcile();
            return fp ? b_->CreateFCmpOGE(l,r) : b_->CreateICmpSGE(l,r);
        }
        case BinaryOp::In: {
            // e IN s  →  (s >> e) & 1  !=  0
            auto* i64 = llvm::Type::getInt64Ty(ctx_);
            auto* shifted = b_->CreateLShr(rhs, lhs);
            auto* bit     = b_->CreateAnd(shifted, llvm::ConstantInt::get(i64,1));
            return b_->CreateICmpNE(bit, llvm::ConstantInt::get(i64,0));
        }
        case BinaryOp::Is:
            error("IS operator requires semantic type information");
        }
    }

    // ---- Designator (variable ref or procedure call) ----
    if (auto* de = dynamic_cast<const DesignatorExpr*>(&e)) {
        if (de->args.has_value())
            return genCallVal(*de);

        // For imported module qualidents try "module_ident" first (covers
        // constants AND variables imported via loadModuleInterface).
        auto* sym = [&]() -> Symbol* {
            if (!de->desig->module.empty() &&
                importedModules_.contains(de->desig->module)) {
                auto* s = lookupSym(de->desig->module + "_" + de->desig->ident);
                if (s) return s;
            }
            return lookupSym(de->desig->ident);
        }();

        // String constant → return i8* directly
        if (sym && sym->isConst && sym->type &&
            sym->type->kind == TypeKind::String) {
            auto* gv = llvm::cast<llvm::GlobalVariable>(sym->llvmVal);
            return strLitPtr(gv);
        }
        // Integer/Real/Bool constant → return value directly
        if (sym && sym->isConst && sym->llvmVal)
            return sym->llvmVal;

        // Variable or parameter: generate address then load
        auto [addr, ty] = genAddr(*de->desig);
        llvm::Type* lt = toLLVM(ty);

        // Arrays: pass as pointer-to-first-element (for procedure args)
        if (ty && ty->kind == TypeKind::Array && !ty->isOpen) {
            auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
            return b_->CreateGEP(lt, addr, {zero, zero}, "arr0");
        }
        return b_->CreateLoad(lt, addr, de->desig->ident);
    }

    error("unsupported expression kind");
}

// -----------------------------------------------------------------------
// Procedure call as an expression (returns the call's value)
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// SYSTEM module intrinsics — function procedures (return a value)
// -----------------------------------------------------------------------
OberonTypePtr CodeGen::tryGetTypeArg(const Expr& e) {
    auto* de = dynamic_cast<const DesignatorExpr*>(&e);
    if (!de || de->args.has_value() || !de->desig->selectors.empty()
            || !de->desig->module.empty())
        return nullptr;
    auto it = typeTable_.find(de->desig->ident);
    return (it != typeTable_.end()) ? it->second : nullptr;
}

llvm::Value* CodeGen::genSysCallVal(const DesignatorExpr& de) {
    auto& d    = *de.desig;
    auto& args = *de.args;
    auto* i64  = llvm::Type::getInt64Ty(ctx_);
    auto* i32  = llvm::Type::getInt32Ty(ctx_);
    auto* i8p  = llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0);

    // ADR(v) → INTEGER  — address of variable v
    // ADR($hh hh...$) → INTEGER — address of inline byte array constant
    if (d.ident == "ADR") {
        if (args.size() != 1) error("SYSTEM.ADR requires one argument");
        // Check for hex byte literal: StringLitExpr with $..$ delimiters
        if (auto* sl = dynamic_cast<const StringLitExpr*>(args[0].get())) {
            const std::string& raw = sl->raw;
            if (raw.size() >= 2 && raw.front() == '$' && raw.back() == '$') {
                // Decode hex pairs from the raw content (delimiters already stripped
                // to leave only hex digits by the lexer, then re-wrapped by parser)
                std::string_view hex(raw.data() + 1, raw.size() - 2);
                std::vector<uint8_t> bytes;
                for (size_t i = 0; i + 1 < hex.size(); i += 2) {
                    auto hexval = [](char c) -> uint8_t {
                        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
                        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
                        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
                        return 0;
                    };
                    bytes.push_back(static_cast<uint8_t>(hexval(hex[i]) * 16 + hexval(hex[i+1])));
                }
                auto* i8t = llvm::Type::getInt8Ty(ctx_);
                auto* arrTy = llvm::ArrayType::get(i8t, bytes.size());
                std::vector<llvm::Constant*> elems;
                elems.reserve(bytes.size());
                for (uint8_t b : bytes)
                    elems.push_back(llvm::ConstantInt::get(i8t, b));
                auto* cArr = llvm::ConstantArray::get(arrTy, elems);
                auto* gv = new llvm::GlobalVariable(
                    *llvmMod_, arrTy, /*isConst=*/true,
                    llvm::GlobalValue::PrivateLinkage, cArr, ".hexbytes");
                auto* i8p = llvm::PointerType::get(i8t, 0);
                auto* ptr = b_->CreateBitCast(gv, i8p);
                return b_->CreatePtrToInt(ptr, i64, "adr");
            }
        }
        auto* de2 = dynamic_cast<const DesignatorExpr*>(args[0].get());
        if (!de2 || de2->args.has_value())
            error("SYSTEM.ADR argument must be a variable, procedure, or hex byte literal");
        // Allow SYSTEM.ADR(Proc) — get the address of a local procedure
        if (de2->desig->selectors.empty() && de2->desig->module.empty()) {
            std::string fname = modName_ + "_" + de2->desig->ident;
            if (auto* fn = llvmMod_->getFunction(fname)) {
                auto* fptr = b_->CreateBitCast(fn, i8p);
                return b_->CreatePtrToInt(fptr, i64, "adr");
            }
        }
        auto [addr, _] = genAddr(*de2->desig);
        return b_->CreatePtrToInt(addr, i64, "adr");
    }

    // SIZE(T) → INTEGER  — byte size of type T (compile-time constant)
    if (d.ident == "SIZE") {
        if (args.size() != 1) error("SYSTEM.SIZE requires one argument");
        OberonTypePtr ot = tryGetTypeArg(*args[0]);
        if (!ot) error("SYSTEM.SIZE argument must be a type name");
        llvm::DataLayout DL(llvmMod_.get());
        uint64_t sz = DL.getTypeAllocSize(toLLVM(ot));
        return llvm::ConstantInt::get(i64, sz);
    }

    // BIT(a, n) → BOOLEAN  — n-th bit of the word at address a
    if (d.ident == "BIT") {
        if (args.size() != 2) error("SYSTEM.BIT requires two arguments");
        auto* a   = coerce(genExpr(*args[0]), i64);
        auto* n   = coerce(genExpr(*args[1]), i32);
        auto* ptr = b_->CreateIntToPtr(a, llvm::PointerType::get(i32, 0));
        auto* word = b_->CreateLoad(i32, ptr, "bit.word");
        auto* shifted = b_->CreateLShr(word, n, "bit.sh");
        auto* bit  = b_->CreateAnd(shifted, llvm::ConstantInt::get(i32, 1));
        return b_->CreateICmpNE(bit, llvm::ConstantInt::get(i32, 0), "bit");
    }

    // VAL(T, x) → T  — type reinterpretation (bitcast / ptr↔int cast)
    if (d.ident == "VAL") {
        if (args.size() != 2) error("SYSTEM.VAL requires two arguments");
        OberonTypePtr targetOT = tryGetTypeArg(*args[0]);
        if (!targetOT) error("SYSTEM.VAL first argument must be a type name");
        llvm::Type*  tgt = toLLVM(targetOT);
        llvm::Value* src = genExpr(*args[1]);
        if (src->getType() == tgt) return src;
        if (src->getType()->isIntegerTy() && tgt->isIntegerTy())
            return b_->CreateIntCast(src, tgt, /*isSigned=*/true, "val");
        if (src->getType()->isIntegerTy() && tgt->isPointerTy())
            return b_->CreateIntToPtr(src, tgt, "val");
        if (src->getType()->isPointerTy() && tgt->isIntegerTy())
            return b_->CreatePtrToInt(src, tgt, "val");
        return b_->CreateBitCast(src, tgt, "val");
    }

    // ADC(m, n) → INTEGER  — add with carry (stub: plain add on x86-64)
    if (d.ident == "ADC") {
        if (args.size() != 2) error("SYSTEM.ADC requires two arguments");
        return b_->CreateAdd(coerce(genExpr(*args[0]), i64),
                             coerce(genExpr(*args[1]), i64), "adc");
    }

    // SBC(m, n) → INTEGER  — subtract with carry (stub: plain sub on x86-64)
    if (d.ident == "SBC") {
        if (args.size() != 2) error("SYSTEM.SBC requires two arguments");
        return b_->CreateSub(coerce(genExpr(*args[0]), i64),
                             coerce(genExpr(*args[1]), i64), "sbc");
    }

    // UML(m, n) → INTEGER  — upper 32 bits of unsigned 64-bit product
    if (d.ident == "UML") {
        if (args.size() != 2) error("SYSTEM.UML requires two arguments");
        auto* m  = b_->CreateZExtOrTrunc(genExpr(*args[0]), i64);
        auto* n  = b_->CreateZExtOrTrunc(genExpr(*args[1]), i64);
        auto* pr = b_->CreateMul(m, n, "uml");
        return b_->CreateAShr(pr, llvm::ConstantInt::get(i64, 32), "uml.hi");
    }

    // COND(n) → BOOLEAN  — condition code test (stub: FALSE on x86-64)
    if (d.ident == "COND") {
        return llvm::ConstantInt::getFalse(ctx_);
    }

    // H(n) → INTEGER  — hardware register H (RISC-specific; stub 0)
    if (d.ident == "H") {
        return llvm::ConstantInt::get(i64, 0);
    }

    // REG(n) → INTEGER  — CPU register read (stub: 0 on hosted x86-64)
    if (d.ident == "REG") {
        return llvm::ConstantInt::get(i64, 0);
    }

    (void)i8p;
    error("unknown SYSTEM function: " + d.ident);
}

// -----------------------------------------------------------------------
// SYSTEM module intrinsics — proper procedures (no return value)
// -----------------------------------------------------------------------
void CodeGen::genSysCall(const ProcCallStmt& s) {
    auto& d    = *s.desig;
    auto& args = *s.args;
    auto* i64  = llvm::Type::getInt64Ty(ctx_);
    auto* i8p  = llvm::PointerType::get(llvm::Type::getInt8Ty(ctx_), 0);

    // GET(a, v)  —  v := mem[a]
    if (d.ident == "GET") {
        if (args.size() != 2) error("SYSTEM.GET requires two arguments");
        auto* addr  = coerce(genExpr(*args[0]), i64);
        // Second arg must be a variable (we need its type and address)
        auto* de2 = dynamic_cast<const DesignatorExpr*>(args[1].get());
        if (!de2 || de2->args.has_value())
            error("SYSTEM.GET second argument must be a variable");
        auto [varAddr, varTy] = genAddr(*de2->desig);
        llvm::Type* lt  = toLLVM(varTy);
        auto* ptr = b_->CreateIntToPtr(addr, llvm::PointerType::get(lt, 0));
        auto* val = b_->CreateLoad(lt, ptr, "get");
        b_->CreateStore(val, varAddr);
        return;
    }

    // PUT(a, x)  —  mem[a] := x
    if (d.ident == "PUT") {
        if (args.size() != 2) error("SYSTEM.PUT requires two arguments");
        auto* addr = coerce(genExpr(*args[0]), i64);
        auto* val  = genExpr(*args[1]);
        auto* ptr  = b_->CreateIntToPtr(
            addr, llvm::PointerType::get(val->getType(), 0));
        b_->CreateStore(val, ptr);
        return;
    }

    // COPY(src, dst, n)  —  copy n bytes from address src to dst
    if (d.ident == "COPY") {
        if (args.size() != 3) error("SYSTEM.COPY requires three arguments");
        auto* src   = b_->CreateIntToPtr(coerce(genExpr(*args[0]), i64), i8p);
        auto* dst   = b_->CreateIntToPtr(coerce(genExpr(*args[1]), i64), i8p);
        auto* nbytes = coerce(genExpr(*args[2]), i64);
        b_->CreateMemCpy(dst, llvm::MaybeAlign(1),
                         src, llvm::MaybeAlign(1), nbytes);
        return;
    }

    // LED(n)  —  display n on LEDs (RISC-specific; no-op on x86-64)
    if (d.ident == "LED") { return; }

    // LDREG(n, v)  —  write CPU register (stub: no-op on hosted x86-64)
    if (d.ident == "LDREG") { return; }

    error("unknown SYSTEM procedure: " + d.ident);
}

// -----------------------------------------------------------------------
// Procedure call as an expression (returns the call's value)
// -----------------------------------------------------------------------
llvm::Value* CodeGen::genCallVal(const DesignatorExpr& de) {
    auto& d = *de.desig;

    // Dispatch SYSTEM function intrinsics
    if (!sysAlias_.empty() && d.module == sysAlias_)
        return genSysCallVal(de);

    // Built-in function procedures
    if (d.module.empty() && de.args) {
        auto& args = *de.args;
        auto* i64 = llvm::Type::getInt64Ty(ctx_);
        auto* dbl = llvm::Type::getDoubleTy(ctx_);

        // ABS(x) — absolute value (integer or real)
        if (d.ident == "ABS" && args.size() == 1) {
            auto* v = genExpr(*args[0]);
            if (v->getType()->isDoubleTy()) {
                auto* fn = llvm::Intrinsic::getDeclaration(
                    llvmMod_.get(), llvm::Intrinsic::fabs, {dbl});
                return b_->CreateCall(fn, {v});
            }
            auto* vi = coerce(v, i64);
            auto* fn = llvm::Intrinsic::getDeclaration(
                llvmMod_.get(), llvm::Intrinsic::abs, {i64});
            return b_->CreateCall(fn, {vi, llvm::ConstantInt::getFalse(ctx_)});
        }

        // ODD(x) — x MOD 2 # 0
        if (d.ident == "ODD" && args.size() == 1) {
            auto* v = coerce(genExpr(*args[0]), i64);
            auto* bit = b_->CreateAnd(v, llvm::ConstantInt::get(i64, 1));
            return b_->CreateICmpNE(bit, llvm::ConstantInt::get(i64, 0));
        }

        // LEN(a) — compile-time array length
        if (d.ident == "LEN" && !args.empty()) {
            if (auto* de0 = dynamic_cast<const DesignatorExpr*>(args[0].get())) {
                if (!de0->args) {
                    auto [addr, ty] = genAddr(*de0->desig);
                    (void)addr;
                    if (ty && ty->kind == TypeKind::Array && !ty->isOpen)
                        return llvm::ConstantInt::get(i64, ty->length);
                }
            }
            error("LEN: argument must be a fixed-length array variable");
        }

        // ORD(c) — ordinal of CHAR or BOOLEAN
        if (d.ident == "ORD" && args.size() == 1)
            return coerce(genExpr(*args[0]), i64);

        // CHR(x) — CHAR from integer
        if (d.ident == "CHR" && args.size() == 1)
            return coerce(genExpr(*args[0]), llvm::Type::getInt8Ty(ctx_));

        // FLOOR(x) — floor of REAL, result INTEGER
        if (d.ident == "FLOOR" && args.size() == 1) {
            auto* v = genExpr(*args[0]);
            auto* fn = llvm::Intrinsic::getDeclaration(
                llvmMod_.get(), llvm::Intrinsic::floor, {dbl});
            return b_->CreateFPToSI(b_->CreateCall(fn, {v}), i64);
        }

        // FLT(x) — REAL from integer
        if (d.ident == "FLT" && args.size() == 1)
            return b_->CreateSIToFP(coerce(genExpr(*args[0]), i64), dbl);

        // ROR(x, n) — rotate right
        if (d.ident == "ROR" && args.size() == 2) {
            auto* x     = coerce(genExpr(*args[0]), i64);
            auto* n     = coerce(genExpr(*args[1]), i64);
            auto* count = b_->CreateURem(n, llvm::ConstantInt::get(i64, 64));
            auto* fn    = llvm::Intrinsic::getDeclaration(
                llvmMod_.get(), llvm::Intrinsic::fshr, {i64});
            return b_->CreateCall(fn, {x, x, count});
        }

        // LSL(x, n) — logical shift left
        if (d.ident == "LSL" && args.size() == 2) {
            auto* x = coerce(genExpr(*args[0]), i64);
            auto* n = coerce(genExpr(*args[1]), i64);
            return b_->CreateShl(x, n);
        }

        // ASR(x, n) — arithmetic shift right
        if (d.ident == "ASR" && args.size() == 2) {
            auto* x = coerce(genExpr(*args[0]), i64);
            auto* n = coerce(genExpr(*args[1]), i64);
            return b_->CreateAShr(x, n);
        }
    }

    // Indirect call through a procedure variable or record field
    {
        bool isFieldCall = !d.module.empty() && !importedModules_.contains(d.module);
        Symbol* procSym  = nullptr;
        if (!isFieldCall && d.module.empty())
            procSym = lookupSym(d.ident);

        if (isFieldCall ||
            (procSym && procSym->type && procSym->type->kind == TypeKind::Procedure)) {
            auto [addr, procTy] = isFieldCall ? genAddr(d)
                                              : AddrResult{procSym->llvmVal, procSym->type};
            if (!procTy || procTy->kind != TypeKind::Procedure)
                error("indirect call on non-procedure type: " + d.ident);
            auto* fnptrLLVM = toLLVM(procTy);
            auto* fnptr     = b_->CreateLoad(fnptrLLVM, addr, "fnptr");
            auto* fty       = llvm::cast<llvm::FunctionType>(
                fnptrLLVM->getPointerElementType());
            std::vector<llvm::Value*> args;
            if (de.args) {
                for (size_t i = 0; i < de.args->size(); ++i) {
                    auto& argExpr = *(*de.args)[i];
                    llvm::Type* expectedTy = (i < fty->getNumParams())
                                             ? fty->getParamType(i) : nullptr;
                    llvm::Value* v = nullptr;
                    if (expectedTy && expectedTy->isPointerTy()) {
                        if (auto* dse = dynamic_cast<const DesignatorExpr*>(&argExpr)) {
                            if (!dse->args) {
                                auto [a, _] = genAddr(*dse->desig);
                                v = coerce(a, expectedTy);
                            }
                        }
                    }
                    if (!v) { v = genExpr(argExpr); if (expectedTy) v = coerce(v, expectedTy); }
                    args.push_back(v);
                }
            }
            return b_->CreateCall(fty, fnptr, args);
        }
    }

    // Resolve function (direct call)
    llvm::Function* fn = nullptr;
    if (!d.module.empty() && importedModules_.contains(d.module)) {
        // Imported: Out.String → Out_String
        std::string key = d.module + "_" + d.ident;
        auto it = extFuncs_.find(key);
        if (it == extFuncs_.end()) error("unknown imported proc: " + key);
        fn = it->second;
    } else {
        std::string key = modName_ + "_" + d.ident;
        fn = llvmMod_->getFunction(key);
        if (!fn) error("unknown procedure: " + d.ident);
    }

    // Build arguments
    std::vector<llvm::Value*> args;
    auto& fty = *fn->getFunctionType();
    if (de.args) {
        for (size_t i = 0; i < de.args->size(); ++i) {
            auto& argExpr = *(*de.args)[i];
            llvm::Value* v = nullptr;
            llvm::Type*  expectedTy = (i < fty.getNumParams())
                                      ? fty.getParamType(i) : nullptr;

            // For pointer-type parameters (VAR or array), pass address
            if (expectedTy && expectedTy->isPointerTy()) {
                if (auto* dse = dynamic_cast<const DesignatorExpr*>(&argExpr)) {
                    if (!dse->args) {
                        auto [addr, _] = genAddr(*dse->desig);
                        v = coerce(addr, expectedTy);
                    }
                }
            }
            if (!v) {
                v = genExpr(argExpr);
                if (expectedTy) v = coerce(v, expectedTy);
            }
            args.push_back(v);
        }
    }

    return b_->CreateCall(fn, args);
}

// -----------------------------------------------------------------------
// Statement generators
// -----------------------------------------------------------------------
void CodeGen::genStmts(const StmtSeq& stmts) {
    for (auto& s : stmts) {
        if (s) genStmt(*s);
        if (blockTerminated()) break;
    }
}

void CodeGen::genStmt(const Stmt& s) {
    if (auto* a = dynamic_cast<const AssignStmt*>(&s))    { genAssign(*a); return; }
    if (auto* c = dynamic_cast<const ProcCallStmt*>(&s))  { genCall(*c);   return; }
    if (auto* i = dynamic_cast<const IfStmt*>(&s))        { genIf(*i);     return; }
    if (auto* cs = dynamic_cast<const CaseStmt*>(&s))     { genCase(*cs);  return; }
    if (auto* w = dynamic_cast<const WhileStmt*>(&s))     { genWhile(*w);  return; }
    if (auto* r = dynamic_cast<const RepeatStmt*>(&s))    { genRepeat(*r); return; }
    if (auto* f = dynamic_cast<const ForStmt*>(&s))       { genFor(*f);    return; }
    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        if (r->value && retAlloca_) {
            auto* v = genExpr(*r->value);
            b_->CreateStore(coerce(v, curFunc_->getReturnType()), retAlloca_);
        }
        b_->CreateBr(exitBlock_);
        return;
    }
}

// assignment = designator ":=" expression
void CodeGen::genAssign(const AssignStmt& s) {
    auto [addr, ty] = genAddr(*s.target);
    auto* val  = genExpr(*s.value);
    auto* tgt  = toLLVM(ty);
    b_->CreateStore(coerce(val, tgt), addr);
}

// ProcedureCall = designator [ActualParameters]
void CodeGen::genCall(const ProcCallStmt& s) {
    auto& d = *s.desig;

    // SYSTEM proper-procedure intrinsics
    if (!sysAlias_.empty() && d.module == sysAlias_ && s.args) {
        genSysCall(s);
        return;
    }

    // INC(v) / INC(v, n)  —  v := v + 1  or  v := v + n
    if (d.module.empty() && d.ident == "INC" && s.args && !s.args->empty()) {
        auto* de0 = dynamic_cast<const DesignatorExpr*>(s.args->at(0).get());
        if (!de0 || de0->args.has_value()) error("INC: first argument must be a variable");
        auto [addr, ty] = genAddr(*de0->desig);
        auto* lt  = toLLVM(ty);
        auto* cur = b_->CreateLoad(lt, addr);
        auto* delta = s.args->size() >= 2
            ? coerce(genExpr(*s.args->at(1)), lt)
            : llvm::ConstantInt::get(lt, 1);
        b_->CreateStore(b_->CreateAdd(cur, delta), addr);
        return;
    }

    // DEC(v) / DEC(v, n)  —  v := v - 1  or  v := v - n
    if (d.module.empty() && d.ident == "DEC" && s.args && !s.args->empty()) {
        auto* de0 = dynamic_cast<const DesignatorExpr*>(s.args->at(0).get());
        if (!de0 || de0->args.has_value()) error("DEC: first argument must be a variable");
        auto [addr, ty] = genAddr(*de0->desig);
        auto* lt  = toLLVM(ty);
        auto* cur = b_->CreateLoad(lt, addr);
        auto* delta = s.args->size() >= 2
            ? coerce(genExpr(*s.args->at(1)), lt)
            : llvm::ConstantInt::get(lt, 1);
        b_->CreateStore(b_->CreateSub(cur, delta), addr);
        return;
    }

    // INCL(s, e)  —  s := s + {e}   (set bit e in the SET variable s)
    if (d.module.empty() && d.ident == "INCL" && s.args && s.args->size() >= 2) {
        auto* de0 = dynamic_cast<const DesignatorExpr*>(s.args->at(0).get());
        if (!de0 || de0->args.has_value()) error("INCL: first argument must be a SET variable");
        auto [addr, ty] = genAddr(*de0->desig);
        auto* i64 = llvm::Type::getInt64Ty(ctx_);
        auto* cur = coerce(b_->CreateLoad(toLLVM(ty), addr), i64);
        auto* bit = b_->CreateShl(llvm::ConstantInt::get(i64, 1),
                                   coerce(genExpr(*s.args->at(1)), i64));
        b_->CreateStore(b_->CreateOr(cur, bit), addr);
        return;
    }

    // EXCL(s, e)  —  s := s - {e}   (clear bit e in the SET variable s)
    if (d.module.empty() && d.ident == "EXCL" && s.args && s.args->size() >= 2) {
        auto* de0 = dynamic_cast<const DesignatorExpr*>(s.args->at(0).get());
        if (!de0 || de0->args.has_value()) error("EXCL: first argument must be a SET variable");
        auto [addr, ty] = genAddr(*de0->desig);
        auto* i64 = llvm::Type::getInt64Ty(ctx_);
        auto* cur = coerce(b_->CreateLoad(toLLVM(ty), addr), i64);
        auto* bit = b_->CreateShl(llvm::ConstantInt::get(i64, 1),
                                   coerce(genExpr(*s.args->at(1)), i64));
        b_->CreateStore(b_->CreateAnd(cur, b_->CreateNot(bit)), addr);
        return;
    }

    // ASSERT(cond)  —  halt if condition is false
    if (d.module.empty() && d.ident == "ASSERT" && s.args && !s.args->empty()) {
        auto* cond = genExpr(*s.args->at(0));
        if (!cond->getType()->isIntegerTy(1))
            cond = b_->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
        auto* failBB = llvm::BasicBlock::Create(ctx_, "assert.fail", curFunc_);
        auto* contBB = llvm::BasicBlock::Create(ctx_, "assert.ok",   curFunc_);
        b_->CreateCondBr(cond, contBB, failBB);
        b_->SetInsertPoint(failBB);
        auto it = extFuncs_.find("Oberon_Trap");
        if (it != extFuncs_.end()) b_->CreateCall(it->second, {});
        b_->CreateUnreachable();
        b_->SetInsertPoint(contBB);
        return;
    }

    // LED(n)  —  RISC-specific display; no-op on x86-64
    if (d.module.empty() && d.ident == "LED") { return; }

    // NEW(p)  –  built-in procedure
    if (d.module.empty() && d.ident == "NEW" && s.args && s.args->size() == 1) {
        auto* argDE = dynamic_cast<const DesignatorExpr*>(s.args->at(0).get());
        if (!argDE || argDE->args) error("NEW requires a simple pointer variable");

        auto [ptrAddr, ptrTy] = genAddr(*argDE->desig);
        if (ptrTy->kind != TypeKind::Pointer || !ptrTy->ptrBase)
            error("NEW argument must be a POINTER type");

        auto* baseLLVM = toLLVM(ptrTy->ptrBase);
        llvm::DataLayout DL(llvmMod_.get());
        uint64_t sz = DL.getTypeAllocSize(baseLLVM);

        auto* sizeVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), sz);
        auto* raw     = b_->CreateCall(extFuncs_.at("Oberon_NEW"), {sizeVal});
        auto* typed   = b_->CreateBitCast(
            raw, llvm::PointerType::get(baseLLVM, 0));
        b_->CreateStore(coerce(typed, toLLVM(ptrTy)), ptrAddr);
        return;
    }

    // Indirect call through a procedure variable or record field.
    // This happens when:
    //   (a) d.module is set but is NOT an imported module — e.g., F.handle(…)
    //       parsed as qualident{module="F", ident="handle"} where F is a variable.
    //   (b) d.module is empty but d.ident is a local symbol of Procedure type —
    //       e.g., a bare procedure-variable call p(…).
    {
        bool isFieldCall = !d.module.empty() && !importedModules_.contains(d.module);
        Symbol* procSym  = nullptr;
        if (!isFieldCall && d.module.empty())
            procSym = lookupSym(d.ident);

        if (isFieldCall ||
            (procSym && procSym->type && procSym->type->kind == TypeKind::Procedure)) {
            auto [addr, procTy] = isFieldCall ? genAddr(d)
                                              : AddrResult{procSym->llvmVal, procSym->type};
            if (!procTy || procTy->kind != TypeKind::Procedure)
                error("indirect call on non-procedure type: " + d.ident);
            auto* fnptrLLVM = toLLVM(procTy);                          // ptr-to-FunctionType
            auto* fnptr     = b_->CreateLoad(fnptrLLVM, addr, "fnptr");
            auto* fty       = llvm::cast<llvm::FunctionType>(
                fnptrLLVM->getPointerElementType());
            std::vector<llvm::Value*> args;
            if (s.args) {
                for (size_t i = 0; i < s.args->size(); ++i) {
                    auto& argExpr = *(*s.args)[i];
                    llvm::Type* expectedTy = (i < fty->getNumParams())
                                             ? fty->getParamType(i) : nullptr;
                    llvm::Value* v = nullptr;
                    if (expectedTy && expectedTy->isPointerTy()) {
                        if (auto* dse = dynamic_cast<const DesignatorExpr*>(&argExpr)) {
                            if (!dse->args) {
                                auto [a, _] = genAddr(*dse->desig);
                                v = coerce(a, expectedTy);
                            }
                        }
                    }
                    if (!v) { v = genExpr(argExpr); if (expectedTy) v = coerce(v, expectedTy); }
                    args.push_back(v);
                }
            }
            b_->CreateCall(fty, fnptr, args);
            return;
        }
    }

    // Regular procedure call — resolve the function and build args directly.
    llvm::Function* fn = nullptr;
    if (!d.module.empty() && importedModules_.contains(d.module)) {
        std::string key = d.module + "_" + d.ident;
        auto it = extFuncs_.find(key);
        if (it != extFuncs_.end()) fn = it->second;
        else error("unknown imported proc: " + key);
    } else {
        fn = llvmMod_->getFunction(modName_ + "_" + d.ident);
        if (!fn) error("unknown procedure: " + d.ident);
    }

    std::vector<llvm::Value*> args;
    auto& fty = *fn->getFunctionType();
    if (s.args) {
        for (size_t i = 0; i < s.args->size(); ++i) {
            auto& argExpr = *(*s.args)[i];
            llvm::Type* expectedTy = (i < fty.getNumParams())
                                     ? fty.getParamType(i) : nullptr;
            llvm::Value* v = nullptr;
            if (expectedTy && expectedTy->isPointerTy()) {
                if (auto* dse = dynamic_cast<const DesignatorExpr*>(&argExpr)) {
                    if (!dse->args) {
                        auto [addr, _] = genAddr(*dse->desig);
                        v = coerce(addr, expectedTy);
                    }
                }
            }
            if (!v) {
                v = genExpr(argExpr);
                if (expectedTy) v = coerce(v, expectedTy);
            }
            args.push_back(v);
        }
    }
    b_->CreateCall(fn, args);
}

// IF … ELSIF … ELSE … END
void CodeGen::genIf(const IfStmt& s) {
    auto* mergeBB = llvm::BasicBlock::Create(ctx_, "if.end", curFunc_);
    // condBB: the block where the next condition check is emitted.
    // Starts as the current insertion block; advances to nextBB each iteration.
    auto* condBB = b_->GetInsertBlock();

    for (size_t i = 0; i < s.branches.size(); ++i) {
        b_->SetInsertPoint(condBB);   // condition check goes here

        auto& branch = s.branches[i];
        bool  isLast = (i == s.branches.size() - 1);

        auto* thenBB = llvm::BasicBlock::Create(ctx_, "if.then", curFunc_);
        auto* nextBB = (isLast && !s.elseBranch)
            ? mergeBB
            : llvm::BasicBlock::Create(ctx_, "if.else", curFunc_);

        auto* cond = genExpr(*branch.cond);
        if (cond->getType()->isIntegerTy() && !cond->getType()->isIntegerTy(1))
            cond = b_->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(),0));
        b_->CreateCondBr(cond, thenBB, nextBB);

        b_->SetInsertPoint(thenBB);
        genStmts(branch.body);
        if (!blockTerminated()) b_->CreateBr(mergeBB);

        condBB = nextBB;   // next iteration (or else branch) emits here
    }

    if (s.elseBranch) {
        b_->SetInsertPoint(condBB);   // = nextBB from the last IF/ELSIF branch
        genStmts(*s.elseBranch);
        if (!blockTerminated()) b_->CreateBr(mergeBB);
    }

    b_->SetInsertPoint(mergeBB);
}

// CASE expression OF case {"|" case} END
void CodeGen::genCase(const CaseStmt& s) {
    auto* expr    = genExpr(*s.expr);
    auto* endBB   = llvm::BasicBlock::Create(ctx_, "case.end", curFunc_);

    auto* curCheckBB = b_->GetInsertBlock(); // we'll chain condition checks
    (void)curCheckBB;

    for (size_t armIdx = 0; armIdx < s.arms.size(); ++armIdx) {
        auto& arm = s.arms[armIdx];
        if (arm.labels.empty()) continue; // empty arm

        auto* armBB  = llvm::BasicBlock::Create(ctx_, "case.arm", curFunc_);
        auto* nextBB = llvm::BasicBlock::Create(ctx_, "case.chk", curFunc_);

        // Build condition: OR of all label ranges
        llvm::Value* cond = llvm::ConstantInt::getFalse(ctx_);
        for (auto& lr : arm.labels) {
            llvm::Value* lo = genExpr(*lr.lo);
            lo = coerce(lo, expr->getType());
            llvm::Value* match = nullptr;
            if (lr.hi) {
                auto* hi = coerce(genExpr(*lr.hi), expr->getType());
                auto* ge = b_->CreateICmpSGE(expr, lo);
                auto* le = b_->CreateICmpSLE(expr, hi);
                match = b_->CreateAnd(ge, le);
            } else {
                match = b_->CreateICmpEQ(expr, lo);
            }
            cond = b_->CreateOr(cond, match);
        }
        b_->CreateCondBr(cond, armBB, nextBB);

        b_->SetInsertPoint(armBB);
        genStmts(arm.body);
        if (!blockTerminated()) b_->CreateBr(endBB);

        b_->SetInsertPoint(nextBB);
    }
    // Fall through to end
    if (!blockTerminated()) b_->CreateBr(endBB);
    b_->SetInsertPoint(endBB);
}

// WHILE {ELSIF} END
void CodeGen::genWhile(const WhileStmt& s) {
    // All ELSIF branches loop back to the top (the first condition)
    auto* startBB = llvm::BasicBlock::Create(ctx_, "while.start", curFunc_);
    auto* endBB   = llvm::BasicBlock::Create(ctx_, "while.end",   curFunc_);

    b_->CreateBr(startBB);
    b_->SetInsertPoint(startBB);

    for (size_t i = 0; i < s.branches.size(); ++i) {
        auto& branch = s.branches[i];
        auto* bodyBB  = llvm::BasicBlock::Create(ctx_, "while.body", curFunc_);
        auto* checkBB = (i + 1 < s.branches.size())
            ? llvm::BasicBlock::Create(ctx_, "while.elsif", curFunc_)
            : endBB;

        auto* cond = genExpr(*branch.cond);
        if (cond->getType()->isIntegerTy() && !cond->getType()->isIntegerTy(1))
            cond = b_->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(),0));
        b_->CreateCondBr(cond, bodyBB, checkBB);

        b_->SetInsertPoint(bodyBB);
        genStmts(branch.body);
        if (!blockTerminated()) b_->CreateBr(startBB);

        if (checkBB != endBB)
            b_->SetInsertPoint(checkBB);
    }

    b_->SetInsertPoint(endBB);
}

// REPEAT StatementSequence UNTIL expression
void CodeGen::genRepeat(const RepeatStmt& s) {
    auto* bodyBB = llvm::BasicBlock::Create(ctx_, "repeat.body", curFunc_);
    auto* endBB  = llvm::BasicBlock::Create(ctx_, "repeat.end",  curFunc_);

    b_->CreateBr(bodyBB);
    b_->SetInsertPoint(bodyBB);
    genStmts(s.body);

    if (!blockTerminated()) {
        auto* cond = genExpr(*s.until);
        if (cond->getType()->isIntegerTy() && !cond->getType()->isIntegerTy(1))
            cond = b_->CreateICmpNE(cond, llvm::ConstantInt::get(cond->getType(),0));
        // UNTIL: exit if cond is TRUE (opposite of WHILE)
        b_->CreateCondBr(cond, endBB, bodyBB);
    }
    b_->SetInsertPoint(endBB);
}

// FOR ident ":=" from TO to [BY step] DO body END
void CodeGen::genFor(const ForStmt& s) {
    // Resolve loop variable
    auto* sym = lookupSym(s.var);
    if (!sym) error("FOR loop variable not found: " + s.var);
    auto* varPtr = sym->llvmVal;
    auto* i64    = llvm::Type::getInt64Ty(ctx_);

    // Initialise loop variable
    auto* fromVal = coerce(genExpr(*s.from), i64);
    b_->CreateStore(fromVal, varPtr);

    auto* toVal   = coerce(genExpr(*s.to), i64);
    int64_t stepV = 1;
    if (s.by) {
        if (auto* il = dynamic_cast<const IntLitExpr*>(s.by.get()))
            stepV = parseIntLit(il->raw);
        else if (auto* ue = dynamic_cast<const UnaryExpr*>(s.by.get())) {
            if (ue->op == UnaryOp::Minus)
                if (auto* il2 = dynamic_cast<const IntLitExpr*>(ue->operand.get()))
                    stepV = -parseIntLit(il2->raw);
        }
    }
    auto* stepConst = llvm::ConstantInt::get(i64, stepV);

    auto* condBB = llvm::BasicBlock::Create(ctx_, "for.cond", curFunc_);
    auto* bodyBB = llvm::BasicBlock::Create(ctx_, "for.body", curFunc_);
    auto* endBB  = llvm::BasicBlock::Create(ctx_, "for.end",  curFunc_);

    b_->CreateBr(condBB);
    b_->SetInsertPoint(condBB);
    {
        auto* k    = b_->CreateLoad(i64, varPtr, "k");
        llvm::Value* cond = (stepV >= 0)
            ? b_->CreateICmpSLE(k, toVal)
            : b_->CreateICmpSGE(k, toVal);
        b_->CreateCondBr(cond, bodyBB, endBB);
    }

    b_->SetInsertPoint(bodyBB);
    genStmts(s.body);
    if (!blockTerminated()) {
        auto* k    = b_->CreateLoad(i64, varPtr, "k");
        auto* next = b_->CreateAdd(k, stepConst);
        b_->CreateStore(next, varPtr);
        b_->CreateBr(condBB);
    }

    b_->SetInsertPoint(endBB);
}

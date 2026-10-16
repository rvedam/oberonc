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
        bool imported = !d.module.empty() && importedModules_.contains(d.module);
        auto* sym = imported ? lookupSym(d.module + "_" + d.ident) : nullptr;
        if (!sym) sym = lookupSym(d.ident);
        if (!sym) error("unknown symbol: " + d.ident);
        ty = sym->type;
    }

    for (auto& sel : d.selectors) {
        if (!ty) break;
        if (auto* fs = std::get_if<FieldSelector>(&sel)) {
            if (ty->kind == TypeKind::Pointer) ty = ty->ptrBase;
            if (!ty) break;
            int idx = ty->fieldIndex(fs->ident);
            if (idx >= 0) ty = ty->fields[idx].type;
        } else if (std::get_if<IndexSelector>(&sel)) {
            if (ty->kind == TypeKind::Array) ty = ty->elem;
        } else if (std::get_if<DerefSelector>(&sel)) {
            if (ty->kind == TypeKind::Pointer) ty = ty->ptrBase;
        } else if (auto* tgs = std::get_if<TypeGuardSelector>(&sel)) {
            OberonTypePtr g = resolveTypeName(tgs->module, tgs->qualident);
            if (g) ty = g;
        }
    }
    return ty;
}

// -----------------------------------------------------------------------
// Address generator  – returns a pointer to the location + its type
// -----------------------------------------------------------------------
CodeGen::AddrResult CodeGen::genAddr(const Designator& d, size_t maxSelectors) {
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

    // Apply selectors (only the first maxSelectors of them)
    size_t nSel = std::min(maxSelectors, d.selectors.size());
    for (size_t si = 0; si < nSel; ++si) {
        auto& sel = d.selectors[si];
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
            // Index as i64: BYTE/CHAR indices zero-extend, INTEGER sign-extends.
            auto* idx = coerce(genExpr(*is->index), llvm::Type::getInt64Ty(ctx_));
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
        } else if (auto* tgs = std::get_if<TypeGuardSelector>(&sel)) {
            // Type guard v(T): reinterpret the current base as a pointer to T.
            // Emit a bitcast so subsequent field GEPs use T's struct layout.
            OberonTypePtr g = resolveTypeName(tgs->module, tgs->qualident);
            if (g) {
                llvm::Type* newLLVMTy = toLLVM(g);
                llvm::Type* newPtrTy  = llvm::PointerType::get(newLLVMTy, 0);
                if (base->getType() != newPtrTy)
                    base = b_->CreateBitCast(base, newPtrTy, "typeguard");
                ty = g;
            }
        }
    }

    return {base, ty};
}

// -----------------------------------------------------------------------
// Expression generator
// -----------------------------------------------------------------------
llvm::Value* CodeGen::genExpr(const Expr& e) {
    // ---- Literals ----
    if (auto* il = dynamic_cast<const IntLitExpr*>(&e))
        return intConst(parseIntLit(il->raw));

    if (auto* rl = dynamic_cast<const RealLitExpr*>(&e))
        return llvm::ConstantFP::get(realTy(), parseRealLit(rl->raw));

    if (auto* sl = dynamic_cast<const StringLitExpr*>(&e)) {
        std::string content = parseStrContent(sl->raw);
        // Hex char constant (e.g. 0X, 0DX): always a CHAR value → return i8
        bool isHexChar = !sl->raw.empty() && sl->raw.back() == 'X';
        if (isHexChar) {
            unsigned char cv = content.empty() ? '\0' : static_cast<unsigned char>(content[0]);
            return llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx_), cv);
        }
        // Single-char string literal is compatible with CHAR (Oberon §8.1)
        if (content.size() == 1)
            return llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx_),
                                          static_cast<unsigned char>(content[0]));
        auto* gv = getOrCreateStrLit(content);
        return strLitPtr(gv);
    }

    if (dynamic_cast<const NilExpr*>(&e))
        return llvm::ConstantPointerNull::get(ptrTy());

    if (dynamic_cast<const TrueExpr*>(&e))
        return llvm::ConstantInt::getTrue(ctx_);

    if (dynamic_cast<const FalseExpr*>(&e))
        return llvm::ConstantInt::getFalse(ctx_);

    // ---- Set literal ----
    if (auto* se = dynamic_cast<const SetExpr*>(&e)) {
        auto* st = intTy();
        unsigned W = st->getBitWidth();
        llvm::Value* result = llvm::ConstantInt::get(st, 0);
        auto* one = llvm::ConstantInt::get(st, 1);
        for (auto& elem : se->elements) {
            llvm::Value* lo = coerce(genExpr(*elem.lo), st);
            if (elem.hi) {
                llvm::Value* hi = coerce(genExpr(*elem.hi), st);
                auto* loC = llvm::dyn_cast<llvm::ConstantInt>(lo);
                auto* hiC = llvm::dyn_cast<llvm::ConstantInt>(hi);
                if (loC && hiC) {
                    uint64_t mask = 0;
                    for (int64_t v = loC->getSExtValue(); v <= hiC->getSExtValue(); ++v)
                        if (v >= 0 && v < static_cast<int64_t>(W)) mask |= (uint64_t(1) << v);
                    result = b_->CreateOr(result, llvm::ConstantInt::get(st, mask));
                } else {
                    // Runtime: {lo..hi} = (-1 << lo) & (-1 >>> (W-1 - hi))
                    auto* allOnes = llvm::ConstantInt::get(st, ~uint64_t(0));
                    auto* top     = llvm::ConstantInt::get(st, W - 1);
                    auto* loMask  = b_->CreateShl (allOnes, lo);
                    auto* hiMask  = b_->CreateLShr(allOnes, b_->CreateSub(top, hi));
                    result = b_->CreateOr(result, b_->CreateAnd(loMask, hiMask));
                }
            } else {
                result = b_->CreateOr(result, b_->CreateShl(one, lo));
            }
        }
        return result;
    }

    // ---- Unary ----
    if (auto* ue = dynamic_cast<const UnaryExpr*>(&e)) {
        bool setOp = ue->op == UnaryOp::Minus && isSetExpr(*ue->operand);
        auto* v = genExpr(*ue->operand);
        switch (ue->op) {
        case UnaryOp::Minus:
            if (v->getType()->isFloatingPointTy()) return b_->CreateFNeg(v);
            if (setOp) return b_->CreateNot(coerce(v, intTy()));    // set complement
            if (v->getType()->isIntegerTy() &&
                v->getType()->getIntegerBitWidth() < intTy()->getBitWidth())
                v = coerce(v, intTy());
            return b_->CreateNeg(v);
        case UnaryOp::Plus:  return v;
        case UnaryOp::Not:   return b_->CreateNot(v);
        }
    }

    // ---- Binary ----
    if (auto* be = dynamic_cast<const BinaryExpr*>(&e)) {
        // IS: the RHS is a type name, not a value.
        if (be->op == BinaryOp::Is) {
            auto* lde = dynamic_cast<const DesignatorExpr*>(be->left.get());
            OberonTypePtr target = tryGetTypeArg(*be->right);
            if (!lde || lde->args || !target)
                error("IS requires a variable on the left and a type on the right");
            return genIsTest(*lde->desig, target);
        }

        // & and OR are conditional (short-circuit) in Oberon.
        if (be->op == BinaryOp::And || be->op == BinaryOp::Or) {
            bool isAnd = be->op == BinaryOp::And;
            auto toBool = [&](llvm::Value* v) -> llvm::Value* {
                if (v->getType()->isIntegerTy(1)) return v;
                return b_->CreateICmpNE(v, llvm::Constant::getNullValue(v->getType()));
            };
            auto* lhs    = toBool(genExpr(*be->left));
            auto* lhsBB  = b_->GetInsertBlock();
            auto* rhsBB  = llvm::BasicBlock::Create(ctx_, isAnd ? "and.rhs" : "or.rhs", curFunc_);
            auto* doneBB = llvm::BasicBlock::Create(ctx_, isAnd ? "and.end" : "or.end", curFunc_);
            if (isAnd) b_->CreateCondBr(lhs, rhsBB, doneBB);
            else       b_->CreateCondBr(lhs, doneBB, rhsBB);
            b_->SetInsertPoint(rhsBB);
            auto* rhs    = toBool(genExpr(*be->right));
            auto* rhsEnd = b_->GetInsertBlock();
            b_->CreateBr(doneBB);
            b_->SetInsertPoint(doneBB);
            auto* phi = b_->CreatePHI(llvm::Type::getInt1Ty(ctx_), 2);
            phi->addIncoming(isAnd ? llvm::ConstantInt::getFalse(ctx_)
                                   : llvm::ConstantInt::getTrue(ctx_), lhsBB);
            phi->addIncoming(rhs, rhsEnd);
            return phi;
        }

        bool isRel = be->op == BinaryOp::Eq || be->op == BinaryOp::Neq ||
                     be->op == BinaryOp::Lt || be->op == BinaryOp::Leq ||
                     be->op == BinaryOp::Gt || be->op == BinaryOp::Geq;

        // String / character-array comparison (lexicographic, up to 0X).
        if (isRel) {
            OberonTypePtr lt = exprType(*be->left), rt = exprType(*be->right);
            if (isCharArray(lt) || isCharArray(rt)) {
                auto* l = genStringOperand(*be->left);
                auto* r = genStringOperand(*be->right);
                if (l->getType()->isPointerTy() && r->getType()->isPointerTy()) {
                    auto* c = b_->CreateCall(getRuntimeHelper("__oberon_strcmp"),
                                             {b_->CreateBitCast(l, ptrTy()),
                                              b_->CreateBitCast(r, ptrTy())});
                    auto* z = llvm::ConstantInt::get(c->getType(), 0);
                    switch (be->op) {
                    case BinaryOp::Eq:  return b_->CreateICmpEQ (c, z);
                    case BinaryOp::Neq: return b_->CreateICmpNE (c, z);
                    case BinaryOp::Lt:  return b_->CreateICmpSLT(c, z);
                    case BinaryOp::Leq: return b_->CreateICmpSLE(c, z);
                    case BinaryOp::Gt:  return b_->CreateICmpSGT(c, z);
                    default:            return b_->CreateICmpSGE(c, z);
                    }
                }
            }
        }

        bool setOp = isSetExpr(*be->left) || isSetExpr(*be->right);
        auto* lhs = genExpr(*be->left);
        auto* rhs = genExpr(*be->right);
        bool fp = lhs->getType()->isFloatingPointTy() || rhs->getType()->isFloatingPointTy();
        if (fp) {
            llvm::Type* ft = realTy();
            if (lhs->getType()->isDoubleTy() || rhs->getType()->isDoubleTy())
                ft = llvm::Type::getDoubleTy(ctx_);
            lhs = lhs->getType()->isIntegerTy() ? b_->CreateSIToFP(lhs, ft) : coerce(lhs, ft);
            rhs = rhs->getType()->isIntegerTy() ? b_->CreateSIToFP(rhs, ft) : coerce(rhs, ft);
        }

        // Integer operands are computed at least at INTEGER width: BYTE/CHAR
        // (i8) and BOOLEAN (i1) zero-extend, narrower INTEGERs sign-extend.
        auto normaliseInts = [&](llvm::Value*& l, llvm::Value*& r) {
            if (!l->getType()->isIntegerTy() || !r->getType()->isIntegerTy()) return;
            unsigned lw = l->getType()->getIntegerBitWidth();
            unsigned rw = r->getType()->getIntegerBitWidth();
            unsigned tgt = std::max({lw, rw, intTy()->getBitWidth()});
            auto* tgtTy  = llvm::Type::getIntNTy(ctx_, tgt);
            l = coerce(l, tgtTy);
            r = coerce(r, tgtTy);
        };

        // Reconcile for comparisons: pointer mismatches → i8*; ints → common width.
        auto reconcile = [&]() -> std::pair<llvm::Value*,llvm::Value*> {
            llvm::Value *l = lhs, *r = rhs;
            if (l->getType()->isPointerTy() && r->getType()->isPointerTy()) {
                if (l->getType() == r->getType()) return {l, r};
                return {b_->CreateBitCast(l, ptrTy()), b_->CreateBitCast(r, ptrTy())};
            }
            if (l->getType()->isIntegerTy() && r->getType()->isIntegerTy()) {
                if (l->getType()->isIntegerTy(1) && r->getType()->isIntegerTy(1))
                    return {l, r};
                normaliseInts(l, r);
            }
            return {l, r};
        };

        switch (be->op) {
        case BinaryOp::Add: {
            if (fp) return b_->CreateFAdd(lhs, rhs);
            auto [l,r] = reconcile();
            return setOp ? b_->CreateOr(l, r) : b_->CreateAdd(l, r);   // SET union
        }
        case BinaryOp::Sub: {
            if (fp) return b_->CreateFSub(lhs, rhs);
            auto [l,r] = reconcile();
            return setOp ? b_->CreateAnd(l, b_->CreateNot(r))          // SET difference
                         : b_->CreateSub(l, r);
        }
        case BinaryOp::Mul: {
            if (fp) return b_->CreateFMul(lhs, rhs);
            auto [l,r] = reconcile();
            return setOp ? b_->CreateAnd(l, r) : b_->CreateMul(l, r);  // SET intersection
        }
        case BinaryOp::Div: {
            if (fp) return b_->CreateFDiv(lhs, rhs);
            // Non-fp /  is always SET symmetric difference (XOR); / on INTEGER is invalid Oberon
            auto [l,r] = reconcile();
            return b_->CreateXor(l, r);
        }
        case BinaryOp::IDiv:
        case BinaryOp::IMod: {
            // Oberon DIV/MOD round toward negative infinity: x = (x DIV y)*y + x MOD y
            // with 0 <= x MOD y < y for y > 0.
            auto [l,r] = reconcile();
            auto* q = b_->CreateSDiv(l, r);
            auto* m = b_->CreateSRem(l, r);
            auto* zero = llvm::ConstantInt::get(l->getType(), 0);
            auto* adjust = b_->CreateAnd(b_->CreateICmpNE(m, zero),
                                         b_->CreateICmpSLT(b_->CreateXor(m, r), zero));
            if (be->op == BinaryOp::IDiv)
                return b_->CreateSub(q, b_->CreateZExt(adjust, l->getType()));
            return b_->CreateSelect(adjust, b_->CreateAdd(m, r), m);
        }
        case BinaryOp::And:
        case BinaryOp::Or:
            break; // handled above (short-circuit)
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
            auto* st      = intTy();
            auto* set     = coerce(rhs, st);
            auto* shifted = b_->CreateLShr(set, coerce(lhs, st));
            auto* bit     = b_->CreateAnd(shifted, llvm::ConstantInt::get(st, 1));
            return b_->CreateICmpNE(bit, llvm::ConstantInt::get(st, 0));
        }
        case BinaryOp::Is:
            break; // handled above
        }
        error("unsupported binary operator");
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

        bool plain = de->desig->selectors.empty() &&
                     (de->desig->module.empty() || importedModules_.contains(de->desig->module));
        // String constant → return i8* directly
        if (plain && sym && sym->isConst && sym->type &&
            sym->type->kind == TypeKind::String) {
            auto* gv = llvm::cast<llvm::GlobalVariable>(sym->llvmVal);
            return strLitPtr(gv);
        }
        // Integer/Real/Bool constant → return value directly
        if (plain && sym && sym->isConst && sym->llvmVal)
            return sym->llvmVal;

        // Variable or parameter: generate address then load
        auto [addr, ty] = genAddr(*de->desig);
        llvm::Type* lt = toLLVM(ty);

        // Arrays: pass as pointer-to-first-element (for procedure args)
        if (ty && ty->kind == TypeKind::Array && !ty->isOpen) {
            auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), 0);
            return b_->CreateGEP(lt, addr, {zero, zero}, "arr0");
        }
        if (ty && ty->kind == TypeKind::Array)   // open array: already element ptr
            return addr;
        return b_->CreateLoad(lt, addr, de->desig->ident);
    }

    error("unsupported expression kind");
}

// -----------------------------------------------------------------------
// Static expression typing (best effort; used to pick SET vs INTEGER ops,
// string comparison, etc.). Returns nullptr when the type is unknown.
// -----------------------------------------------------------------------
bool CodeGen::isCharArray(const OberonTypePtr& t) {
    return t && (t->kind == TypeKind::String ||
                 (t->kind == TypeKind::Array && t->elem && t->elem->kind == TypeKind::Char));
}

bool CodeGen::isSetExpr(const Expr& e) {
    auto t = exprType(e);
    return t && t->kind == TypeKind::Set;
}

OberonTypePtr CodeGen::exprType(const Expr& e) {
    if (dynamic_cast<const SetExpr*>(&e))    return typeTable_["SET"];
    if (dynamic_cast<const IntLitExpr*>(&e)) return typeTable_["INTEGER"];
    if (dynamic_cast<const RealLitExpr*>(&e)) return typeTable_["REAL"];
    if (dynamic_cast<const TrueExpr*>(&e) || dynamic_cast<const FalseExpr*>(&e))
        return typeTable_["BOOLEAN"];
    if (auto* sl = dynamic_cast<const StringLitExpr*>(&e)) {
        bool isChar = (!sl->raw.empty() && sl->raw.back() == 'X') ||
                      parseStrContent(sl->raw).size() == 1;
        return isChar ? typeTable_["CHAR"] : typeTable_["STRING"];
    }
    if (auto* ue = dynamic_cast<const UnaryExpr*>(&e)) {
        if (ue->op == UnaryOp::Not) return typeTable_["BOOLEAN"];
        return exprType(*ue->operand);
    }
    if (auto* be = dynamic_cast<const BinaryExpr*>(&e)) {
        switch (be->op) {
        case BinaryOp::Add: case BinaryOp::Sub: case BinaryOp::Mul: case BinaryOp::Div: {
            auto l = exprType(*be->left);
            if (l && l->kind == TypeKind::Set) return l;
            auto r = exprType(*be->right);
            if (r && r->kind == TypeKind::Set) return r;
            if (be->op == BinaryOp::Div) return typeTable_["REAL"];
            return l ? l : r;
        }
        case BinaryOp::IDiv: case BinaryOp::IMod: return typeTable_["INTEGER"];
        default: return typeTable_["BOOLEAN"];
        }
    }
    if (auto* de = dynamic_cast<const DesignatorExpr*>(&e)) {
        const auto& d = *de->desig;
        if (de->args) {
            if (!sysAlias_.empty() && d.module == sysAlias_) {
                if (d.ident == "VAL" && !de->args->empty())
                    return tryGetTypeArg(*de->args->at(0));
                if (d.ident == "BIT" || d.ident == "COND") return typeTable_["BOOLEAN"];
                return typeTable_["INTEGER"];
            }
            if (d.module.empty()) {
                static const char* intFns[] = {"ORD", "LEN", "FLOOR", "ODD"};
                for (auto* f : intFns)
                    if (d.ident == f) return typeTable_[d.ident == "ODD" ? "BOOLEAN" : "INTEGER"];
                if (d.ident == "CHR") return typeTable_["CHAR"];
                if (d.ident == "FLT") return typeTable_["REAL"];
                if ((d.ident == "ABS" || d.ident == "LSL" || d.ident == "ASR" ||
                     d.ident == "ROR") && !de->args->empty())
                    return exprType(*de->args->at(0));
            }
            // Type guard used as an expression: v(T)
            if (de->args->size() == 1)
                if (auto t = tryGetTypeArg(*de->args->at(0));
                    t && (t->kind == TypeKind::Record || t->kind == TypeKind::Pointer))
                    return t;
            // Function procedure: declared return type
            llvm::Function* fn = nullptr;
            if (!d.module.empty() && importedModules_.contains(d.module)) {
                auto it = extFuncs_.find(d.module + "_" + d.ident);
                if (it != extFuncs_.end()) fn = it->second;
            } else if (d.module.empty()) {
                fn = llvmMod_->getFunction(modName_ + "_" + d.ident);
            }
            if (fn) {
                auto it = procRetTypes_.find(fn);
                return it != procRetTypes_.end() ? it->second : nullptr;
            }
            try {   // procedure variable / field
                auto t = typeOfDesig(d);
                return (t && t->kind == TypeKind::Procedure) ? t->retType : nullptr;
            } catch (const std::runtime_error&) { return nullptr; }
        }
        try { return typeOfDesig(d); } catch (const std::runtime_error&) { return nullptr; }
    }
    return nullptr;
}

// A string-valued operand: string literals (even one character long) become
// a pointer to a NUL-terminated constant; arrays decay to element pointers.
llvm::Value* CodeGen::genStringOperand(const Expr& e) {
    if (auto* sl = dynamic_cast<const StringLitExpr*>(&e)) {
        std::string content = parseStrContent(sl->raw);
        if (content == std::string(1, '\0')) content.clear();   // 0X = empty string
        return strLitPtr(getOrCreateStrLit(content));
    }
    if (auto* de = dynamic_cast<const DesignatorExpr*>(&e)) {
        if (!de->args) {
            // A CHAR constant compared with an array is a one-character string.
            auto* sym = lookupSym(de->desig->ident);
            if (de->desig->selectors.empty() && de->desig->module.empty() &&
                sym && sym->isConst && sym->type && sym->type->kind == TypeKind::Char) {
                auto* ci = llvm::cast<llvm::ConstantInt>(sym->llvmVal);
                std::string content(1, static_cast<char>(ci->getZExtValue()));
                if (content[0] == '\0') content.clear();
                return strLitPtr(getOrCreateStrLit(content));
            }
        }
    }
    return genExpr(e);
}

// -----------------------------------------------------------------------
// Run-time type information
// -----------------------------------------------------------------------
llvm::StructType* CodeGen::tdStructTy() {
    llvm::Type* fields[] = {ptrTy()};
    return llvm::StructType::get(ctx_, llvm::ArrayRef<llvm::Type*>(fields));
}

llvm::Value* CodeGen::tdRef(const OberonTypePtr& recTy) {
    if (!recTy || recTy->kind != TypeKind::Record || recTy->tdName.empty())
        return llvm::ConstantPointerNull::get(ptrTy());
    auto* gv = llvmMod_->getGlobalVariable(recTy->tdName);
    if (!gv)
        gv = new llvm::GlobalVariable(*llvmMod_, tdStructTy(), /*isConst=*/true,
                                      llvm::GlobalValue::ExternalLinkage,
                                      nullptr, recTy->tdName);
    return llvm::ConstantExpr::getBitCast(gv, ptrTy());
}

void CodeGen::emitTypeDescriptor(const OberonTypePtr& recTy) {
    if (!recTy || recTy->kind != TypeKind::Record || recTy->tdName.empty()) return;
    llvm::Constant* base = llvm::ConstantPointerNull::get(ptrTy());
    if (!recTy->baseName.empty()) {
        OberonTypePtr bt = resolveTypeName(recTy->baseModule, recTy->baseName);
        if (bt && bt->kind == TypeKind::Pointer) bt = bt->ptrBase;
        base = llvm::cast<llvm::Constant>(tdRef(bt));
    }
    tdRef(recTy);   // ensure the global exists
    auto* gv = llvmMod_->getGlobalVariable(recTy->tdName);
    gv->setInitializer(llvm::ConstantStruct::get(tdStructTy(), {base}));
}

// Dynamic type tag of a record-typed designator.
llvm::Value* CodeGen::genRecordTag(const Designator& d, const OberonTypePtr& staticTy) {
    size_t n = d.selectors.size();
    while (n > 0 && std::holds_alternative<TypeGuardSelector>(d.selectors[n - 1])) --n;
    // VAR record parameter: its dynamic type arrives as a hidden argument.
    if (n == 0 && d.module.empty())
        if (auto* sym = lookupSym(d.ident); sym && sym->tagVal) return sym->tagVal;
    // p^: the tag lives in the heap header in front of the record.
    if (n > 0 && std::holds_alternative<DerefSelector>(d.selectors[n - 1])) {
        auto [pa, pty] = genAddr(d, n - 1);
        auto* p = b_->CreateLoad(toLLVM(pty), pa, "p");
        return b_->CreateCall(getRuntimeHelper("__oberon_ptrtag"),
                              {b_->CreateBitCast(p, ptrTy())});
    }
    // Otherwise the dynamic type is the static type.
    OberonTypePtr t = staticTy;
    if (t && t->kind == TypeKind::Pointer) t = t->ptrBase;
    return tdRef(t);
}

llvm::Value* CodeGen::genIsTest(const Designator& d, const OberonTypePtr& target) {
    OberonTypePtr targetRec = target->kind == TypeKind::Pointer ? target->ptrBase : target;
    auto [addr, ty] = genAddr(d);
    llvm::Value* tag;
    if (ty && ty->kind == TypeKind::Pointer) {
        auto* p = b_->CreateLoad(toLLVM(ty), addr, "p");
        tag = b_->CreateCall(getRuntimeHelper("__oberon_ptrtag"),
                             {b_->CreateBitCast(p, ptrTy())});
    } else {
        tag = genRecordTag(d, ty);
    }
    return b_->CreateCall(getRuntimeHelper("__oberon_is"), {tag, tdRef(targetRec)});
}

// Small helper functions emitted (once, privately) into each module that
// needs them, so the runtime libraries need no new entry points.
llvm::Function* CodeGen::getRuntimeHelper(const std::string& name) {
    if (auto* f = llvmMod_->getFunction(name)) return f;
    auto* i8  = llvm::Type::getInt8Ty(ctx_);
    auto* i32 = llvm::Type::getInt32Ty(ctx_);
    auto* i64 = llvm::Type::getInt64Ty(ctx_);
    auto* i1  = llvm::Type::getInt1Ty(ctx_);
    auto* p   = ptrTy();
    llvm::IRBuilder<> b(ctx_);
    auto mk = [&](llvm::Type* ret, std::vector<llvm::Type*> ps) {
        auto* fn = llvm::Function::Create(llvm::FunctionType::get(ret, ps, false),
                                          llvm::Function::InternalLinkage, name, llvmMod_.get());
        return fn;
    };

    if (name == "__oberon_strcmp") {
        // int strcmp(a, b) over unsigned bytes, stopping at 0X
        auto* fn = mk(i32, {p, p});
        auto* entry = llvm::BasicBlock::Create(ctx_, "entry", fn);
        auto* loop  = llvm::BasicBlock::Create(ctx_, "loop", fn);
        auto* same  = llvm::BasicBlock::Create(ctx_, "same", fn);
        auto* diff  = llvm::BasicBlock::Create(ctx_, "diff", fn);
        auto* end   = llvm::BasicBlock::Create(ctx_, "end", fn);
        b.SetInsertPoint(entry);
        b.CreateBr(loop);
        b.SetInsertPoint(loop);
        auto* i  = b.CreatePHI(i64, 2);
        i->addIncoming(llvm::ConstantInt::get(i64, 0), entry);
        auto* ca = b.CreateZExt(b.CreateLoad(i8, b.CreateGEP(i8, fn->getArg(0), i)), i32);
        auto* cb = b.CreateZExt(b.CreateLoad(i8, b.CreateGEP(i8, fn->getArg(1), i)), i32);
        b.CreateCondBr(b.CreateICmpEQ(ca, cb), same, diff);
        b.SetInsertPoint(same);
        i->addIncoming(b.CreateAdd(i, llvm::ConstantInt::get(i64, 1)), same);
        b.CreateCondBr(b.CreateICmpEQ(ca, llvm::ConstantInt::get(i32, 0)), end, loop);
        b.SetInsertPoint(diff);
        b.CreateRet(b.CreateSub(ca, cb));
        b.SetInsertPoint(end);
        b.CreateRet(llvm::ConstantInt::get(i32, 0));
        return fn;
    }
    if (name == "__oberon_is") {
        // bool is(tag, target): walk the descriptor's base chain
        auto* fn = mk(i1, {p, p});
        auto* entry = llvm::BasicBlock::Create(ctx_, "entry", fn);
        auto* loop  = llvm::BasicBlock::Create(ctx_, "loop", fn);
        auto* chk   = llvm::BasicBlock::Create(ctx_, "check", fn);
        auto* yes   = llvm::BasicBlock::Create(ctx_, "yes", fn);
        auto* no    = llvm::BasicBlock::Create(ctx_, "no", fn);
        b.SetInsertPoint(entry);
        b.CreateBr(loop);
        b.SetInsertPoint(loop);
        auto* t = b.CreatePHI(p, 2);
        t->addIncoming(fn->getArg(0), entry);
        b.CreateCondBr(b.CreateIsNull(t), no, chk);
        b.SetInsertPoint(chk);
        auto* next = b.CreateLoad(p, t);   // td->base is the first field
        t->addIncoming(next, chk);
        b.CreateCondBr(b.CreateICmpEQ(t, fn->getArg(1)), yes, loop);
        b.SetInsertPoint(yes); b.CreateRet(llvm::ConstantInt::getTrue(ctx_));
        b.SetInsertPoint(no);  b.CreateRet(llvm::ConstantInt::getFalse(ctx_));
        return fn;
    }
    if (name == "__oberon_ptrtag") {
        // i8* ptrtag(p): NIL → NIL, else the descriptor stored at p - 8
        auto* fn = mk(p, {p});
        auto* entry = llvm::BasicBlock::Create(ctx_, "entry", fn);
        auto* load  = llvm::BasicBlock::Create(ctx_, "load", fn);
        auto* nil   = llvm::BasicBlock::Create(ctx_, "nil", fn);
        b.SetInsertPoint(entry);
        b.CreateCondBr(b.CreateIsNull(fn->getArg(0)), nil, load);
        b.SetInsertPoint(load);
        auto* hdr = b.CreateGEP(i8, fn->getArg(0), llvm::ConstantInt::get(i64, -8));
        b.CreateRet(b.CreateLoad(p, hdr));
        b.SetInsertPoint(nil);
        b.CreateRet(llvm::ConstantPointerNull::get(p));
        return fn;
    }
    error("unknown runtime helper " + name);
}

// -----------------------------------------------------------------------
// Procedure call as an expression (returns the call's value)
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// SYSTEM module intrinsics — function procedures (return a value)
// -----------------------------------------------------------------------
OberonTypePtr CodeGen::tryGetTypeArg(const Expr& e) {
    auto* de = dynamic_cast<const DesignatorExpr*>(&e);
    if (!de || de->args.has_value() || !de->desig->selectors.empty())
        return nullptr;
    // Try qualified key first (e.g. "FileDir_FileHd" for FileDir.FileHd).
    if (!de->desig->module.empty()) {
        auto it = typeTable_.find(de->desig->module + "_" + de->desig->ident);
        if (it != typeTable_.end()) return it->second;
        return nullptr;
    }
    auto it = typeTable_.find(de->desig->ident);
    return (it != typeTable_.end()) ? it->second : nullptr;
}

llvm::Value* CodeGen::genSysCallVal(const DesignatorExpr& de) {
    auto& d    = *de.desig;
    auto& args = *de.args;
    auto* i64  = llvm::Type::getInt64Ty(ctx_);
    auto* i32  = llvm::Type::getInt32Ty(ctx_);
    auto* it   = intTy();
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
                auto* ptr = b_->CreateBitCast(gv, i8p);
                return b_->CreatePtrToInt(ptr, it, "adr");
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
                return b_->CreatePtrToInt(fptr, it, "adr");
            }
        }
        auto [addr, _] = genAddr(*de2->desig);
        return b_->CreatePtrToInt(addr, it, "adr");
    }

    // SIZE(T) → INTEGER  — byte size of type T (compile-time constant)
    if (d.ident == "SIZE") {
        if (args.size() != 1) error("SYSTEM.SIZE requires one argument");
        OberonTypePtr ot = tryGetTypeArg(*args[0]);
        if (!ot) error("SYSTEM.SIZE argument must be a type name");
        llvm::DataLayout DL(llvmMod_.get());
        uint64_t sz = DL.getTypeAllocSize(toLLVM(ot));
        return llvm::ConstantInt::get(it, sz);
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
            return coerce(src, tgt);
        // REAL <-> INTEGER/SET: reinterpret the bit pattern
        if (src->getType()->isFloatingPointTy() && tgt->isIntegerTy())
            return coerce(b_->CreateBitCast(src, llvm::Type::getIntNTy(
                ctx_, src->getType()->getPrimitiveSizeInBits())), tgt);
        if (src->getType()->isIntegerTy() && tgt->isFloatingPointTy())
            return b_->CreateBitCast(coerce(src, llvm::Type::getIntNTy(
                ctx_, tgt->getPrimitiveSizeInBits())), tgt);
        if (src->getType()->isIntegerTy() && tgt->isPointerTy())
            return b_->CreateIntToPtr(src, tgt, "val");
        if (src->getType()->isPointerTy() && tgt->isIntegerTy())
            return b_->CreatePtrToInt(src, tgt, "val");
        // Aggregate target (struct/array): ptr already points to the right memory.
        // Return it as-is; caller will pass it as a pointer argument.
        if (src->getType()->isPointerTy() && (tgt->isStructTy() || tgt->isArrayTy()))
            return src;
        return b_->CreateBitCast(src, tgt, "val");
    }

    // ADC(m, n) → INTEGER  — add with carry (stub: plain add on x86-64)
    if (d.ident == "ADC") {
        if (args.size() != 2) error("SYSTEM.ADC requires two arguments");
        return b_->CreateAdd(coerce(genExpr(*args[0]), it),
                             coerce(genExpr(*args[1]), it), "adc");
    }

    // SBC(m, n) → INTEGER  — subtract with carry (stub: plain sub on x86-64)
    if (d.ident == "SBC") {
        if (args.size() != 2) error("SYSTEM.SBC requires two arguments");
        return b_->CreateSub(coerce(genExpr(*args[0]), it),
                             coerce(genExpr(*args[1]), it), "sbc");
    }

    // UML(m, n) → INTEGER  — upper 32 bits of unsigned 64-bit product
    if (d.ident == "UML") {
        if (args.size() != 2) error("SYSTEM.UML requires two arguments");
        auto* m  = b_->CreateZExtOrTrunc(genExpr(*args[0]), i64);
        auto* n  = b_->CreateZExtOrTrunc(genExpr(*args[1]), i64);
        auto* pr = b_->CreateMul(m, n, "uml");
        return coerce(b_->CreateLShr(pr, llvm::ConstantInt::get(i64, 32), "uml.hi"), it);
    }

    // COND(n) → BOOLEAN  — condition code test (stub: FALSE on x86-64)
    if (d.ident == "COND") {
        return llvm::ConstantInt::getFalse(ctx_);
    }

    // H(n) → INTEGER  — hardware register H (RISC-specific; stub 0)
    if (d.ident == "H") {
        return llvm::ConstantInt::get(it, 0);
    }

    // REG(n) → INTEGER  — CPU register read (stub: 0 on hosted x86-64)
    if (d.ident == "REG") {
        return llvm::ConstantInt::get(it, 0);
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

    // PUT(a, x)  —  mem[a] := x   (the size of x's type decides the access width)
    if (d.ident == "PUT") {
        if (args.size() != 2) error("SYSTEM.PUT requires two arguments");
        auto* addr = coerce(genExpr(*args[0]), i64);
        auto* val  = genExpr(*args[1]);
        if (val->getType()->isIntegerTy(1))
            val = coerce(val, llvm::Type::getInt8Ty(ctx_));      // BOOLEAN: one byte
        else if (val->getType()->isIntegerTy() &&
                 val->getType()->getIntegerBitWidth() > intTy()->getBitWidth())
            val = coerce(val, intTy());                           // INTEGER: one word
        auto* ptr  = b_->CreateIntToPtr(
            addr, llvm::PointerType::get(val->getType(), 0));
        b_->CreateStore(val, ptr);
        return;
    }

    // COPY(src, dst, n)  —  copy n words from address src to dst
    if (d.ident == "COPY") {
        if (args.size() != 3) error("SYSTEM.COPY requires three arguments");
        auto* src   = b_->CreateIntToPtr(coerce(genExpr(*args[0]), i64), i8p);
        auto* dst   = b_->CreateIntToPtr(coerce(genExpr(*args[1]), i64), i8p);
        auto* nbytes = b_->CreateMul(coerce(genExpr(*args[2]), i64),
                                     llvm::ConstantInt::get(i64, intTy()->getBitWidth() / 8));
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
        auto* it  = intTy();
        unsigned W = it->getBitWidth();

        // ABS(x) — absolute value (integer or real)
        if (d.ident == "ABS" && args.size() == 1) {
            auto* v = genExpr(*args[0]);
            if (v->getType()->isFloatingPointTy()) {
                auto* fn = llvm::Intrinsic::getDeclaration(
                    llvmMod_.get(), llvm::Intrinsic::fabs, {v->getType()});
                return b_->CreateCall(fn, {v});
            }
            auto* vi = coerce(v, it);
            auto* fn = llvm::Intrinsic::getDeclaration(
                llvmMod_.get(), llvm::Intrinsic::abs, {it});
            return b_->CreateCall(fn, {vi, llvm::ConstantInt::getFalse(ctx_)});
        }

        // ODD(x) — x MOD 2 # 0
        if (d.ident == "ODD" && args.size() == 1) {
            auto* v = coerce(genExpr(*args[0]), it);
            auto* bit = b_->CreateAnd(v, llvm::ConstantInt::get(it, 1));
            return b_->CreateICmpNE(bit, llvm::ConstantInt::get(it, 0));
        }

        // LEN(a) — array length (compile-time for fixed arrays, runtime for open arrays)
        if (d.ident == "LEN" && !args.empty()) {
            if (auto* de0 = dynamic_cast<const DesignatorExpr*>(args[0].get())) {
                if (!de0->args) {
                    auto ty = typeOfDesig(*de0->desig);
                    if (ty && ty->kind == TypeKind::Array)
                        return coerce(genLength(*args[0], ty), it);
                }
            }
            error("LEN: argument must be an array variable");
        }

        // ORD(c) — ordinal of CHAR, BOOLEAN, SET, REAL, or pointer (ptrtoint)
        if (d.ident == "ORD" && args.size() == 1) {
            auto* v = genExpr(*args[0]);
            if (v->getType()->isFloatingPointTy())   // the REAL's bit pattern
                return coerce(b_->CreateBitCast(v, llvm::Type::getIntNTy(
                    ctx_, v->getType()->getPrimitiveSizeInBits()), "ord"), it);
            if (v->getType()->isPointerTy())
                return b_->CreatePtrToInt(v, it, "ord");
            return coerce(v, it);
        }

        // CHR(x) — CHAR from integer
        if (d.ident == "CHR" && args.size() == 1)
            return coerce(genExpr(*args[0]), llvm::Type::getInt8Ty(ctx_));

        // FLOOR(x) — floor of REAL, result INTEGER
        if (d.ident == "FLOOR" && args.size() == 1) {
            auto* v = coerce(genExpr(*args[0]), realTy());
            auto* fn = llvm::Intrinsic::getDeclaration(
                llvmMod_.get(), llvm::Intrinsic::floor, {v->getType()});
            return b_->CreateFPToSI(b_->CreateCall(fn, {v}), it);
        }

        // FLT(x) — REAL from integer
        if (d.ident == "FLT" && args.size() == 1)
            return b_->CreateSIToFP(coerce(genExpr(*args[0]), it), realTy());

        // LSL / ASR / ROR operate on a full INTEGER word. The shift count is
        // taken modulo the word size, as on RISC5 (so ASR(x, -k) shifts by W-k).
        bool isShift = d.ident == "LSL" || d.ident == "ASR" || d.ident == "ROR";
        if (isShift && args.size() == 2) {
            auto* x = coerce(genExpr(*args[0]), it);
            auto* n = b_->CreateAnd(coerce(genExpr(*args[1]), it),
                                    llvm::ConstantInt::get(it, W - 1));
            if (d.ident == "LSL") return b_->CreateShl(x, n);
            if (d.ident == "ASR") return b_->CreateAShr(x, n);
            auto* fn = llvm::Intrinsic::getDeclaration(
                llvmMod_.get(), llvm::Intrinsic::fshr, {it});
            return b_->CreateCall(fn, {x, x, n});
        }
    }

    // Type guard in expression position: v(T) or v.field(T) where T is a type.
    // The parser can't distinguish this from a call when ) is followed by END/ELSE/etc.
    // Detect by checking whether the single "argument" resolves to a type, not a value.
    if (de.args && de.args->size() == 1) {
        auto* argDE = dynamic_cast<const DesignatorExpr*>(de.args->at(0).get());
        if (argDE && !argDE->args && argDE->desig->selectors.empty()) {
            std::string guardKey = argDE->desig->module.empty()
                ? argDE->desig->ident
                : argDE->desig->module + "_" + argDE->desig->ident;
            auto guardIt = typeTable_.find(guardKey);
            if (guardIt == typeTable_.end() && !argDE->desig->module.empty())
                guardIt = typeTable_.find(argDE->desig->ident);
            if (guardIt != typeTable_.end() &&
                (guardIt->second->kind == TypeKind::Record ||
                 guardIt->second->kind == TypeKind::Pointer)) {
                OberonTypePtr guardType = guardIt->second;
                auto [addr, baseTy] = genAddr(d);
                llvm::Value* val = b_->CreateLoad(toLLVM(baseTy), addr, "tg_val");
                return b_->CreateBitCast(val, toLLVM(guardType), "typeguard");
            }
        }
    }

    // Indirect call through a procedure variable or record field
    // (excludes direct procedure references registered with a Function llvmVal)
    {
        // Also treat as a field call when an imported module's variable is
        // accessed through selectors (e.g. Oberon.FocusViewer.handle(...)).
        bool isFieldCall = !d.module.empty() &&
                           (!importedModules_.contains(d.module) || !d.selectors.empty());
        Symbol* procSym  = nullptr;
        if (!isFieldCall && d.module.empty())
            procSym = lookupSym(d.ident);

        if (isFieldCall ||
            (procSym && procSym->type && procSym->type->kind == TypeKind::Procedure &&
             !llvm::isa<llvm::Function>(procSym->llvmVal))) {
            auto [addr, procTy] = isFieldCall ? genAddr(d)
                                              : AddrResult{procSym->llvmVal, procSym->type};
            if (!procTy || procTy->kind != TypeKind::Procedure)
                error("indirect call on non-procedure type: " + d.ident);
            auto* fnptrLLVM = toLLVM(procTy);
            auto* fty       = toLLVMFunctionType(procTy);
            auto* fnptr     = b_->CreateLoad(fnptrLLVM, addr, "fnptr");
            auto  fs        = makeFormals(procTy->params);
            auto  args      = genArgs(de.args ? &*de.args : nullptr, fty, &fs);
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
    auto fit = formals_.find(fn);
    auto args = genArgs(de.args ? &*de.args : nullptr, fn->getFunctionType(),
                        fit != formals_.end() ? &fit->second : nullptr);
    return b_->CreateCall(fn, args);
}

// Element count (i64) of an array-valued actual parameter.
llvm::Value* CodeGen::genLength(const Expr& arrayArg, OberonTypePtr argTy) {
    auto* i64 = llvm::Type::getInt64Ty(ctx_);
    if (auto* sl = dynamic_cast<const StringLitExpr*>(&arrayArg)) {
        std::string content = parseStrContent(sl->raw);
        if (content == std::string(1, '\0')) content.clear();
        return llvm::ConstantInt::get(i64, content.size() + 1);
    }
    if (argTy && argTy->kind == TypeKind::Array) {
        if (!argTy->isOpen) return llvm::ConstantInt::get(i64, argTy->length);
        if (auto* dse = dynamic_cast<const DesignatorExpr*>(&arrayArg))
            if (dse->desig->selectors.empty())
                if (Symbol* lenSym = lookupSym("__len_" + dse->desig->ident))
                    return b_->CreateLoad(i64, lenSym->llvmVal, "len");
    }
    if (argTy && argTy->kind == TypeKind::String) return llvm::ConstantInt::get(i64, 0);
    return llvm::ConstantInt::get(i64, 0);
}

// Builds the LLVM argument list for a call. `formals` describes the callee's
// Oberon parameters (nullptr for runtime functions with plain C signatures).
std::vector<llvm::Value*> CodeGen::genArgs(const std::vector<ExprPtr>* actuals,
                                           llvm::FunctionType* fty,
                                           const std::vector<Formal>* formals) {
    std::vector<llvm::Value*> args;
    if (!actuals) return args;
    size_t li = 0;   // LLVM parameter index (skips hidden arguments)
    for (size_t i = 0; i < actuals->size(); ++i) {
        const Expr& argExpr = *(*actuals)[i];
        const Formal* f = (formals && i < formals->size()) ? &(*formals)[i] : nullptr;
        llvm::Type* expectedTy = li < fty->getNumParams() ? fty->getParamType(li) : nullptr;
        auto* dse = dynamic_cast<const DesignatorExpr*>(&argExpr);
        auto* sle = dynamic_cast<const StringLitExpr*>(&argExpr);
        bool byRef = f ? f->byRef : (expectedTy && expectedTy->isPointerTy() && dse &&
                                     !dse->args && [&] {
                                         auto t = exprType(argExpr);
                                         return t && (t->kind == TypeKind::Array ||
                                                      t->kind == TypeKind::Record);
                                     }());
        llvm::Value* v = nullptr;
        OberonTypePtr argOT;
        if (sle && (byRef || (expectedTy && expectedTy->isPointerTy()))) {
            v = genStringOperand(argExpr);            // string literal → char array
        } else if (byRef && dse && !dse->args) {
            auto [addr, aty] = genAddr(*dse->desig);
            v = addr;
            argOT = aty;
        } else {
            v = genExpr(argExpr);
            if (byRef && !v->getType()->isPointerTy()) {
                // VAR param with an rvalue (e.g. SYSTEM.VAL result): pass a temp.
                auto* tmp = createEntryAlloca(curFunc_, v->getType(), "var_tmp");
                b_->CreateStore(v, tmp);
                v = tmp;
            }
        }
        if (expectedTy) v = coerce(v, expectedTy);
        args.push_back(v);
        ++li;

        if (f && f->hidden == Hidden::Len) {
            if (!argOT) argOT = exprType(argExpr);
            args.push_back(genLength(argExpr, argOT));
            ++li;
        } else if (f && f->hidden == Hidden::Tag) {
            llvm::Value* tag = (dse && !dse->args) ? genRecordTag(*dse->desig, argOT ? argOT : f->type)
                                                   : tdRef(f->type);
            args.push_back(tag);
            ++li;
        }
    }
    return args;
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
    if (ty && (ty->kind == TypeKind::Array || ty->kind == TypeKind::Record)) {
        llvm::Value* dstLen = nullptr;   // element count of an open-array target
        if (ty->kind == TypeKind::Array && ty->isOpen && s.target->selectors.empty())
            if (Symbol* lenSym = lookupSym("__len_" + s.target->ident))
                dstLen = b_->CreateLoad(llvm::Type::getInt64Ty(ctx_), lenSym->llvmVal, "len");
        genArrayOrRecordCopy(addr, ty, *s.value, dstLen);
        return;
    }
    auto* val  = genExpr(*s.value);
    auto* tgt  = toLLVM(ty);
    b_->CreateStore(coerce(val, tgt), addr);
}

// Structured assignment: copy bytes into dst. Strings copy up to and including
// their 0X; arrays and records copy min(size(dst), size(src)) bytes (records
// assign the projection onto the destination's static type).
void CodeGen::genArrayOrRecordCopy(llvm::Value* dst, OberonTypePtr dstTy, const Expr& src,
                                   llvm::Value* dstLen) {
    auto* i64 = llvm::Type::getInt64Ty(ctx_);
    llvm::DataLayout DL(llvmMod_.get());
    auto sizeOf = [&](const OberonTypePtr& t) -> llvm::Value* {
        if (t->kind == TypeKind::Array && t->isOpen) return nullptr;
        return llvm::ConstantInt::get(i64, DL.getTypeAllocSize(toLLVM(t)));
    };
    llvm::Value* dstSize = sizeOf(dstTy);
    if (!dstSize && dstLen)
        dstSize = b_->CreateMul(dstLen, llvm::ConstantInt::get(
                                    i64, DL.getTypeAllocSize(toLLVM(dstTy->elem))));
    if (!dstSize) error("assignment to open array of unknown length");

    llvm::Value* srcPtr  = nullptr;
    llvm::Value* srcSize = nullptr;
    if (auto* sl = dynamic_cast<const StringLitExpr*>(&src)) {
        srcPtr  = genStringOperand(src);
        srcSize = genLength(*sl, nullptr);
    } else if (auto* de = dynamic_cast<const DesignatorExpr*>(&src); de && !de->args) {
        auto* sym = lookupSym(de->desig->ident);
        if (de->desig->selectors.empty() && sym && sym->isConst && sym->type &&
            sym->type->kind == TypeKind::String) {
            srcPtr  = genStringOperand(src);
            auto* gv = llvm::cast<llvm::GlobalVariable>(sym->llvmVal);
            srcSize = llvm::ConstantInt::get(i64, DL.getTypeAllocSize(gv->getValueType()));
        } else {
            auto [a, sty] = genAddr(*de->desig);
            srcPtr = a;
            if (sty && sty->kind == TypeKind::Array && sty->isOpen) {
                auto* elemSz = llvm::ConstantInt::get(i64, DL.getTypeAllocSize(toLLVM(sty->elem)));
                srcSize = b_->CreateMul(genLength(src, sty), elemSz);
            } else if (sty) {
                srcSize = sizeOf(sty);
            }
        }
    } else {
        // e.g. SYSTEM.VAL(T, x) yielding an aggregate pointer
        srcPtr = genExpr(src);
        if (!srcPtr->getType()->isPointerTy()) {
            b_->CreateStore(coerce(srcPtr, toLLVM(dstTy)), dst);
            return;
        }
    }
    llvm::Value* n = dstSize;
    if (srcSize)
        n = b_->CreateSelect(b_->CreateICmpULT(srcSize, dstSize), srcSize, dstSize);
    b_->CreateMemCpy(dst, llvm::MaybeAlign(1), srcPtr, llvm::MaybeAlign(1), n);
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
        auto* st  = toLLVM(ty);
        auto* cur = b_->CreateLoad(st, addr);
        auto* bit = b_->CreateShl(llvm::ConstantInt::get(st, 1),
                                   coerce(genExpr(*s.args->at(1)), st));
        b_->CreateStore(b_->CreateOr(cur, bit), addr);
        return;
    }

    // EXCL(s, e)  —  s := s - {e}   (clear bit e in the SET variable s)
    if (d.module.empty() && d.ident == "EXCL" && s.args && s.args->size() >= 2) {
        auto* de0 = dynamic_cast<const DesignatorExpr*>(s.args->at(0).get());
        if (!de0 || de0->args.has_value()) error("EXCL: first argument must be a SET variable");
        auto [addr, ty] = genAddr(*de0->desig);
        auto* st  = toLLVM(ty);
        auto* cur = b_->CreateLoad(st, addr);
        auto* bit = b_->CreateShl(llvm::ConstantInt::get(st, 1),
                                   coerce(genExpr(*s.args->at(1)), st));
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

        // Every heap object gets an 8-byte header holding its type descriptor
        // (NIL for non-records); the pointer refers to the object after it.
        auto* i64     = llvm::Type::getInt64Ty(ctx_);
        auto* sizeVal = llvm::ConstantInt::get(i64, sz + 8);
        auto* raw     = b_->CreateCall(extFuncs_.at("Oberon_NEW"), {sizeVal});
        b_->CreateStore(tdRef(ptrTy->ptrBase),
                        b_->CreateBitCast(raw, llvm::PointerType::get(this->ptrTy(), 0)));
        auto* obj     = b_->CreateGEP(llvm::Type::getInt8Ty(ctx_), raw,
                                      llvm::ConstantInt::get(i64, 8), "obj");
        auto* typed   = b_->CreateBitCast(
            obj, llvm::PointerType::get(baseLLVM, 0));
        b_->CreateStore(coerce(typed, toLLVM(ptrTy)), ptrAddr);
        return;
    }

    // Indirect call through a procedure variable or record field.
    // This happens when:
    //   (a) d.module is set but is NOT an imported module — e.g., F.handle(…)
    //       parsed as qualident{module="F", ident="handle"} where F is a variable.
    //   (b) d.module is an imported module BUT the designator has selectors —
    //       e.g., Oberon.FocusViewer.handle(…) where FocusViewer is a module VAR.
    //   (c) d.module is empty but d.ident is a local symbol of Procedure type —
    //       e.g., a bare procedure-variable call p(…).
    // Note: symbols registered for direct procedures (llvmVal is a Function, not
    // an alloca/global) are excluded — those take the direct-call path below.
    {
        bool isFieldCall = !d.module.empty() &&
                           (!importedModules_.contains(d.module) || !d.selectors.empty());
        Symbol* procSym  = nullptr;
        if (!isFieldCall && d.module.empty())
            procSym = lookupSym(d.ident);

        if (isFieldCall ||
            (procSym && procSym->type && procSym->type->kind == TypeKind::Procedure &&
             !llvm::isa<llvm::Function>(procSym->llvmVal))) {
            auto [addr, procTy] = isFieldCall ? genAddr(d)
                                              : AddrResult{procSym->llvmVal, procSym->type};
            if (!procTy || procTy->kind != TypeKind::Procedure)
                error("indirect call on non-procedure type: " + d.ident);
            auto* fnptrLLVM = toLLVM(procTy);
            auto* fty       = toLLVMFunctionType(procTy);
            auto* fnptr     = b_->CreateLoad(fnptrLLVM, addr, "fnptr");
            auto  fs        = makeFormals(procTy->params);
            b_->CreateCall(fty, fnptr, genArgs(s.args ? &*s.args : nullptr, fty, &fs));
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
    auto fit = formals_.find(fn);
    b_->CreateCall(fn, genArgs(s.args ? &*s.args : nullptr, fn->getFunctionType(),
                               fit != formals_.end() ? &fit->second : nullptr));
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
    auto* endBB = llvm::BasicBlock::Create(ctx_, "case.end", curFunc_);

    // Detect type-guard CASE: labels are type names (record/pointer types),
    // not integer values. Oberon uses this for message-type dispatch.
    auto isTypeLabel = [&](const Expr& e) -> bool {
        auto* de = dynamic_cast<const DesignatorExpr*>(&e);
        if (!de || de->args || !de->desig->selectors.empty()) return false;
        std::string key = de->desig->module.empty()
            ? de->desig->ident
            : de->desig->module + "_" + de->desig->ident;
        return typeTable_.count(key) > 0 ||
               (!de->desig->module.empty() && typeTable_.count(de->desig->ident) > 0);
    };

    bool typeGuard = !s.arms.empty() &&
                     !s.arms[0].labels.empty() &&
                     isTypeLabel(*s.arms[0].labels[0].lo);

    if (typeGuard) {
        // CASE v OF T1: … | T2: … END — test v's dynamic type against each
        // arm's type in order; within an arm, v has that arm's type.
        auto* cde = dynamic_cast<const DesignatorExpr*>(s.expr.get());
        if (!cde || cde->args) error("type CASE requires a variable");
        const Designator& cd = *cde->desig;
        Symbol* sym = (cd.selectors.empty() && cd.module.empty()) ? lookupSym(cd.ident) : nullptr;

        for (auto& arm : s.arms) {
            if (arm.labels.empty()) continue;
            OberonTypePtr guardType;
            llvm::Value* cond = llvm::ConstantInt::getFalse(ctx_);
            for (auto& lr : arm.labels) {
                OberonTypePtr t = tryGetTypeArg(*lr.lo);
                if (!t) error("type CASE label is not a type");
                if (!guardType) guardType = t;
                cond = b_->CreateOr(cond, genIsTest(cd, t));
            }
            auto* armBB  = llvm::BasicBlock::Create(ctx_, "tcase.arm", curFunc_);
            auto* nextBB = llvm::BasicBlock::Create(ctx_, "tcase.next", curFunc_);
            b_->CreateCondBr(cond, armBB, nextBB);

            b_->SetInsertPoint(armBB);
            Symbol saved;
            if (sym) { saved = *sym; sym->type = guardType; }
            genStmts(arm.body);
            if (sym) *sym = saved;
            if (!blockTerminated()) b_->CreateBr(endBB);

            b_->SetInsertPoint(nextBB);
        }
        if (!blockTerminated()) b_->CreateBr(endBB);
        b_->SetInsertPoint(endBB);
        return;
    }

    // Integer/CHAR CASE: standard switch via chained comparisons
    auto* expr = coerce(genExpr(*s.expr), intTy());   // CHAR zero-extends
    for (size_t armIdx = 0; armIdx < s.arms.size(); ++armIdx) {
        auto& arm = s.arms[armIdx];
        if (arm.labels.empty()) continue;

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
    auto* i64    = toLLVM(sym->type);   // the control variable's own width

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

#include "codegen.hpp"

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>

// -----------------------------------------------------------------------
// Type-of helper  (used to know which LLVM ops to emit)
// -----------------------------------------------------------------------
OberonTypePtr CodeGen::typeOfDesig(const Designator& d) {
    OberonTypePtr ty;

    if (!d.module.empty() && !importedModules_.count(d.module)) {
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

    if (!d.module.empty() && !importedModules_.count(d.module)) {
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
        auto* sym = lookupSym(d.ident);
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
        auto* gv = getOrCreateStrLit(parseStrContent(sl->raw));
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
                if (loC && hiC) {
                    int64_t mask = 0;
                    for (int64_t v = loC->getSExtValue();
                             v <= hiC->getSExtValue(); ++v)
                        mask |= (int64_t(1) << v);
                    result = b_->CreateOr(result, llvm::ConstantInt::get(i64, mask));
                } else {
                    error("non-constant set range not yet supported");
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
        bool fp = lhs->getType()->isDoubleTy();
        switch (be->op) {
        case BinaryOp::Add:  return fp ? b_->CreateFAdd(lhs,rhs) : b_->CreateAdd(lhs,rhs);
        case BinaryOp::Sub:  return fp ? b_->CreateFSub(lhs,rhs) : b_->CreateSub(lhs,rhs);
        case BinaryOp::Mul:  return fp ? b_->CreateFMul(lhs,rhs) : b_->CreateMul(lhs,rhs);
        case BinaryOp::Div:  return b_->CreateFDiv(lhs,rhs);
        case BinaryOp::IDiv: return b_->CreateSDiv(lhs,rhs);
        case BinaryOp::IMod: return b_->CreateSRem(lhs,rhs);
        case BinaryOp::And:  return b_->CreateAnd(lhs,rhs);
        case BinaryOp::Or:   return b_->CreateOr (lhs,rhs);
        case BinaryOp::Eq:
            return fp ? b_->CreateFCmpOEQ(lhs,rhs) : b_->CreateICmpEQ (lhs,rhs);
        case BinaryOp::Neq:
            return fp ? b_->CreateFCmpONE(lhs,rhs) : b_->CreateICmpNE (lhs,rhs);
        case BinaryOp::Lt:
            return fp ? b_->CreateFCmpOLT(lhs,rhs) : b_->CreateICmpSLT(lhs,rhs);
        case BinaryOp::Leq:
            return fp ? b_->CreateFCmpOLE(lhs,rhs) : b_->CreateICmpSLE(lhs,rhs);
        case BinaryOp::Gt:
            return fp ? b_->CreateFCmpOGT(lhs,rhs) : b_->CreateICmpSGT(lhs,rhs);
        case BinaryOp::Geq:
            return fp ? b_->CreateFCmpOGE(lhs,rhs) : b_->CreateICmpSGE(lhs,rhs);
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

        auto* sym = lookupSym(de->desig->ident);

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
llvm::Value* CodeGen::genCallVal(const DesignatorExpr& de) {
    auto& d = *de.desig;

    // Resolve function
    llvm::Function* fn = nullptr;
    if (!d.module.empty() && importedModules_.count(d.module)) {
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

    // Regular procedure call — resolve the function and build args directly.
    llvm::Function* fn = nullptr;
    if (!d.module.empty() && importedModules_.count(d.module)) {
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

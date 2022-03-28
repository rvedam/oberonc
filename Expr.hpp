#pragma once

#include "llvm/IR/LLVMContext.h"

// base class for our AST (All syntax nodes will be based on this)
class Expr
{
    public:
        virtual ~Expr() = default;

        // every expression type must be able to generate its IR representation
        virtual llvm::Value* codegen() = 0;
};

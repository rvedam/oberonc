#pragma once

#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>

// base class for our AST (All syntax nodes will be based on this)
class Expr
{
public:
    virtual ~Expr() = default;
};

class ModuleExpr : public Expr
{
private:
    // module can contain
    std::vector<Expr*> m_children;

public:
    ModuleExpr(std::string name);
    ~ModuleExpr();

};
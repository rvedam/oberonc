#pragma once

#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>
#include <vector>

// base class for our AST (All syntax nodes will be based on this)
class Expr
{
public:
    virtual ~Expr() = default;

};

class IntegerExpr : public Expr
{
private:
    int m_val;
public:
    IntegerExpr(int val) : m_val(val) {}
};

class RealNumberExpr : public Expr
{
private:
    double m_val;
public:
    RealNumberExpr(double val) : m_val(val) {}
    double value() const { return m_val; }
};

class ModuleExpr : public Expr
{
private:
    std::string m_name;
    std::vector<Expr*> m_children;
public:
    ModuleExpr(std::string name) : m_name(name) {}
    ~ModuleExpr();
    void addChild(Expr* child) {
        m_children.emplace_back(child);
    }
};


#pragma once

#include "Symbol.hpp"

/**
 * @brief This is the symbol representation of Function, what its inputs are and what it returns
 */
class FunctionSymbol : public Symbol
{
private:
    std::vector<Symbol> m_args;
    Symbol m_return;
public:
    explicit FunctionSymbol(std::string name, std::vector<Symbol> args, bool exported);
    std::vector<Symbol> const& args() const;
    /**
     * @brief return value of the function
     * @return
     */
    Symbol returnValue() const;
    /**
     * @brief returns the string reprsentation of the function signature with types only
     * @return std::string representing the function signature with types in the form (f : I -> O)
     */
    std::string functionSignature() const;
};

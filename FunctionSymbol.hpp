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
  /**
   * @brief constructor of FunctionSymbol
   * @param name
   * @param args
   * @param returnType
   * @param variable
   * @param exported
   */
    explicit FunctionSymbol(std::string name,
                            std::vector<Symbol>&& args,
                            Symbol&& returnType,
                            const bool variable,
                            const bool exported);

  /**
   * @brief returns the vector containing the argument symbols of the function
   * @return std::vector<Symbol> representing of the arguments of the function
   */
  std::vector<Symbol> const& args() const;
    /**
     * @brief return value of the function
     * @return
     */
    Symbol returnValue() const;
    /**
     * @brief returns the string representation of the function signature with types only
     * @return std::string representing the function signature with types in the form (f : I -> O)
     */
    std::string functionSignature() const;

};

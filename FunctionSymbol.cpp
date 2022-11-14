//
// Created by vedam on 10/19/22.
//

#include <sstream>
#include <algorithm>
#include "Symbol.hpp"
#include "FunctionSymbol.hpp"

FunctionSymbol::FunctionSymbol(std::string name,
                               std::vector<Symbol> &&mArgs,
                               Symbol &&mReturn, bool variable, bool exported) :
  Symbol(name, SymbolType::PROCEDURE_TYPE, variable,exported),
  m_args(std::move(mArgs)),
  m_return(std::move(mReturn)) {

}


const std::vector<Symbol> &FunctionSymbol::args() const
{
  return m_args;
}

Symbol FunctionSymbol::returnValue() const
{
  return m_return;
}

std::string FunctionSymbol::functionSignature() const
{
  std::ostringstream oss;
  oss << name() << "(";
  std::vector<std::string> args;
  std::for_each(m_args.cbegin(), m_args.cend(), [&oss](Symbol v) {
    oss << v.name() << ": " << toString(v.type());
  });
  return oss.str();
}

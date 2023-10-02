//
// Created by vedam on 10/21/22.
//

#include "PointerTypeSymbol.hpp"

PointerSymbol::PointerSymbol(std::string name, std::string moduleName, Symbol pointerType, bool variable,
                             bool exported) :
    Symbol(name, moduleName, SymbolType::POINTER_TYPE, variable, exported),
    m_pointerType(pointerType)
{

}

Symbol PointerSymbol::pointerType() const
{
  return m_pointerType;
}

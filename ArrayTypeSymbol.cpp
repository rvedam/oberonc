//
// Created by vedam on 10/20/22.
//

#include "ArrayTypeSymbol.hpp"

ArrayTypeSymbol::ArrayTypeSymbol(std::string name,
                                 Symbol arrayType,
                                 int arraySize,
                                 bool isDynamic,
                                 bool exported) :
                                 Symbol(name, SymbolType::ARRAY_TYPE, false, exported),
                                 m_arrayType(arrayType),
                                 m_arraySize(arraySize),
                                 m_dynamic(isDynamic) {}

bool ArrayTypeSymbol::isDynamic() const
{
  return m_dynamic;
}

int ArrayTypeSymbol::size() const
{
  return m_arraySize;
}

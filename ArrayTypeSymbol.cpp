//
// Created by vedam on 10/20/22.
//

#include "ArrayTypeSymbol.h"

ArrayTypeSymbol::ArrayTypeSymbol(std::string name,
                                 Symbol arrayType,
                                 int arraySize,
                                 bool isDynamic,
                                 bool exported) :
                                 Symbol(name, SymbolType::ARRAY_TYPE, false, exported),
                                 m_arrayType(arrayType),
                                 m_arraySize(arraySize),
                                 m_dynamic(isDynamic) {}

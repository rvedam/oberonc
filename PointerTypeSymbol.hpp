//
// Created by vedam on 10/21/22.
//

#pragma once

#include "Symbol.hpp"

class PointerSymbol : public Symbol
{
private:
  Symbol m_pointerType;
public:
  explicit PointerSymbol(std::string name, std::string moduleName, Symbol pointerType, bool variable,
                         bool exported);
  Symbol pointerType() const;
};
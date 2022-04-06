//
// Created by Ramnarayan Vedam on 8/26/20.
//

#pragma once
#include <string>
#include <ostream>
#include "SymbolType.hpp"

class Symbol
{
private:
  std::string m_name;           // name of symbol
  oberon::SymbolType m_type;    // type of symbol
  bool m_exported;              // whether symbol is exported
public:
  std::string name() const;

  oberon::SymbolType type() const;

  friend std::ostream &operator<<(std::ostream &os, const Symbol &symbol);
};

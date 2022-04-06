//
// Created by Ramnarayan Vedam on 8/26/20.
//

#include "Symbol.h"

std::string Symbol::name() const { return m_name; }

oberon::SymbolType Symbol::type() const
{
  return m_type;
}

std::ostream &operator<<(std::ostream &os, const Symbol &symbol)
{
  os << "Symbol<";
  os << "m_name: " << symbol.m_name << ", m_type: " << symbol.m_type;
  os << ">";
  return os;
}

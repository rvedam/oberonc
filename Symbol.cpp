//
// Created by vedam on 4/20/22.
//

#include "Symbol.hpp"

Symbol::Symbol(std::string name, std::string module, SymbolType type, bool variable, bool exported) :
    m_name(name),
    m_module(module),
    m_exported(exported),
    m_variable(variable),
    m_type(type)
{
}

Symbol::~Symbol() noexcept {}

std::string Symbol::name() const { return m_name; }

std::ostream &operator<<(std::ostream &os, const Symbol &symbol)
{
    os << "Symbol<";
    os << "name: " << symbol.m_name;
    os << ", type: " << typeid(symbol).name();
    os << ", exported: " << symbol.m_exported;
    os << ">";
    return os;
}

std::string toString(SymbolType type) {
  switch (type) {
    case SymbolType::INTEGER:
      return "INTEGER";
    case SymbolType::REAL:
      return "REAL";
    case SymbolType::ARRAY_TYPE:
      return "ARRAY OF";
    case SymbolType::POINTER_TYPE:
      return "POINTER TO ";
    case SymbolType::RECORD_TYPE:
      return "RECORD WITH ";
    default:
      return "UNKNOWN TYPE";
  }
}

bool Symbol::exported() const
{
  return false;
}

bool Symbol::variable() const
{
  return false;
}

SymbolType Symbol::type() const
{
  return m_type;
}

std::string Symbol::module() const
{
  return m_module;
}

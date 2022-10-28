//
// Created by vedam on 4/20/22.
//

#include "Symbol.hpp"

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

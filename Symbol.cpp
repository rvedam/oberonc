//
// Created by vedam on 4/20/22.
//

#include "Symbol.hpp"
#include "SymbolType.hpp"

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

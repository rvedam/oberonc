//
// Created by Ramnarayan Vedam on 8/26/20.
//

#pragma once

#include <string>
#include <ostream>
#include <vector>

enum class SymbolType
{
    INTEGER,
    REAL,
    ARRAY_TYPE,
    POINTER_TYPE,
    RECORD_TYPE,
    PROCEDURE_TYPE
};

std::string toString(SymbolType type);

class Symbol
{
private:
    std::string m_name;             // name of symbol
    bool m_exported;                // whether symbol is exported
    bool m_variable;                // whether symbol represents a variable
    SymbolType m_type;              // underlying type of symbol
    std::string m_userDefinedType;  // user-defined type
public:
    explicit Symbol(std::string name, SymbolType type, bool variable, bool exported) :
    m_name(name),
    m_exported(exported),
    m_variable(variable),
    m_type(type) {}

    std::string name() const;
    bool exported() const;
    bool variable() const;
    SymbolType type() const;
    friend std::ostream &operator<<(std::ostream &os, const Symbol &symbol);
};

class RecordTypeSymbol : public Symbol
{
private:
    std::vector<Symbol> m_members;
public:
    explicit RecordTypeSymbol(std::string name, std::vector<Symbol> members, bool exported);
    std::vector<Symbol> const& members() const;
};

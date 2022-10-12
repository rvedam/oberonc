#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

class Symbol;

class SymbolTable
{
public:
    SymbolTable();
    ~SymbolTable();
    bool exists(std::string identifier) const; // returns true if Symbol exists in Symbol table
    void add(Symbol&& symbol); // move symbols into symbol table to transfer ownership
    void addModuleExport(std::string name);
private:
    std::vector<std::string> m_exportedModules;
    std::unordered_map<std::string, std::unique_ptr<Symbol>> scope;
    std::vector<std::unique_ptr<SymbolTable>> inner_scopes;
};

#include "SymbolTable.hpp"
#include "Symbol.hpp"

bool SymbolTable::exists(std::string identifier) const
{
    return false;
}

void SymbolTable::add(std::unique_ptr<Symbol> symbol)
{
  m_scope.push_back(std::move(symbol));
}

void SymbolTable::addModuleImport(std::string name)
{

}

SymbolTable::~SymbolTable()
{
  m_importedModules.clear();
  m_scope.clear();
}

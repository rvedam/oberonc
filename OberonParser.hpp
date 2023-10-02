//
// Created by vedam on 7/10/23.
//

#pragma once

#include "SymbolTable.hpp"
#include "Expr.hpp"

class ErrorReporter;
class Tokenizer;

class OberonParser
{
private:
  SymbolTable symbols;
  std::unique_ptr<Expr> m_ast;
  ErrorReporter* reporter;
  void parseConsts();
  void parseTypes();
  void parseProcedures();
public:
  explicit OberonParser(ErrorReporter* reporter);
  Expr* ast() const;
  SymbolTable symbolTable() const;
  void parseModule(Tokenizer* tokenizer);
};

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
  std::unique_ptr<Expr> ast;
  std::unique_ptr<ErrorReporter> reporter;
  void parseConsts();
  void parseTypes();
  void parseProcedures();
  void parseComments();
public:
  explicit OberonParser(ErrorReporter* reporter);
  void parseModule(Tokenizer* tokenizer);
};

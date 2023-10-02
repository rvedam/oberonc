//
// Created by vedam on 7/10/23.
//

#include "OberonParser.hpp"
#include "ErrorReporter.hpp"
#include "Tokenizer.hpp"

void OberonParser::parseConsts()
{

}

void OberonParser::parseTypes()
{

}

void OberonParser::parseProcedures()
{

}

OberonParser::OberonParser(ErrorReporter *reporter) : reporter(reporter)
{

}

void OberonParser::parseModule(Tokenizer *tokenizer)
{
  auto token = tokenizer->getNextToken();
  if(token.getTokenType() != TokenType::MODULE) {
    reporter->report(token.getFilename(), token.getLine(), token.getColumn(), "missing MODULE at location");
    return;
  }
  // check to see if the next parameter is IDENTIFIER otherwise report error and return.
  if(tokenizer->peek().getTokenType() != TokenType::IDENTIFIER) {
    token = tokenizer->peek();
    reporter->report(token.getFilename(), token.getLine(), token.getColumn(), "missing IDENTIFIER at location");
    return;
  }

  // eat the identifier so that we can create the ModuleExpr
  token = tokenizer->getNextToken();

  // check to see if the next token is a semi-colon. if so, we have the beginnings of a valid module.
  // Otherwise, report that we are missing a semi-colon and return
  if(tokenizer->peek().getTokenType() != TokenType::SEMICOLON) {
    token = tokenizer->peek();
    reporter->report(token.getFilename(), token.getLine(),
                     token.getColumn(), "missing ';' at location");
    return;
  }

  // eat the semicolon since it indicates the end of the module declaration.
  tokenizer->getNextToken();

  m_ast.reset(new ModuleExpr(token.getParsedString()));
}

Expr *OberonParser::ast() const
{
  return m_ast.get();
}

SymbolTable OberonParser::symbolTable() const
{
  return symbols;
}

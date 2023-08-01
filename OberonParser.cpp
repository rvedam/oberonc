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

  }
}

void OberonParser::parseComments()
{

}

#pragma once

#include "antlr4-runtime.h"
#include "OberonVisitor.h"

class CstVisitor : public oberon::OberonVisitor
{
  public:
    /* handles defining identifiers*/
    virtual antlrcpp::Any visitIdentifier(oberon::OberonParser::IdentifierContext* context) override;
    virtual antlrcpp::Any visitQualifiedIdentifier(oberon::OberonParser::QualifiedIdentifierContext* context) override;
};

#pragma once

#include "antlr4-runtime.h"
#include "OberonBaseVisitor.h"
#include "OberonParser.h"
#include "SymbolTable.hpp"
#include "Expr.hpp"

class ErrorReporter;
using namespace oberon;

class CstVisitor : public oberon::OberonBaseVisitor
{
private:
    ErrorReporter* m_reporter;
    SymbolTable m_programScope;
    std::unique_ptr<ModuleExpr> m_tree;
public:
    explicit CstVisitor(ErrorReporter* reporter);
    ~CstVisitor() = default;
    ModuleExpr* ast() const;

public:
    /* handles defining identifiers*/
    virtual antlrcpp::Any visitQualifiedIdentifier(OberonParser::QualifiedIdentifierContext *context) override;
    virtual antlrcpp::Any visitExportedIdentifier(OberonParser::ExportedIdentifierContext *context) override;
    virtual antlrcpp::Any visitModule(OberonParser::ModuleContext *context) override;

};

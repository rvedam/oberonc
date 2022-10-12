
#include "CstVisitor.hpp"
#include "ErrorReporter.hpp"

CstVisitor::CstVisitor(ErrorReporter *reporter)
{
}

ModuleExpr *CstVisitor::ast() const
{
    return nullptr;
}

antlrcpp::Any CstVisitor::visitQualifiedIdentifier(OberonParser::QualifiedIdentifierContext *context)
{
    return visitChildren(context);
}

antlrcpp::Any CstVisitor::visitExportedIdentifier(OberonParser::ExportedIdentifierContext *context)
{
    return visitChildren(context);
}

antlrcpp::Any CstVisitor::visitModule(OberonParser::ModuleContext *context)
{
    return visitChildren(context);
}

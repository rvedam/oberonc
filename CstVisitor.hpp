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
    virtual antlrcpp::Any visitInteger(OberonParser::IntegerContext *ctx) override;
    virtual antlrcpp::Any visitReal(OberonParser::RealContext *ctx) override;
    virtual antlrcpp::Any visitString(OberonParser::StringContext *ctx) override;
    virtual antlrcpp::Any visitComment(OberonParser::CommentContext *ctx) override;
    virtual antlrcpp::Any visitIdentifierAssignment(OberonParser::IdentifierAssignmentContext *ctx) override;
    virtual antlrcpp::Any visitConstDeclaration(OberonParser::ConstDeclarationContext *ctx) override;
    virtual antlrcpp::Any visitParseExpression(OberonParser::ParseExpressionContext *ctx) override;
    virtual antlrcpp::Any visitParseSimpleExpression(OberonParser::ParseSimpleExpressionContext *ctx) override;
    virtual antlrcpp::Any visitTerm(OberonParser::TermContext *ctx) override;
    virtual antlrcpp::Any visitFactor(OberonParser::FactorContext *ctx) override;
    virtual antlrcpp::Any visitRelation(OberonParser::RelationContext *ctx) override;
    virtual antlrcpp::Any visitParseAddOperator(OberonParser::ParseAddOperatorContext *ctx) override;
    virtual antlrcpp::Any visitMultOperator(OberonParser::MultOperatorContext *ctx) override;
    virtual antlrcpp::Any visitImprt(OberonParser::ImprtContext *ctx) override;
    virtual antlrcpp::Any visitImportList(OberonParser::ImportListContext *ctx) override;
    virtual antlrcpp::Any visitModule(OberonParser::ModuleContext *context) override;
};

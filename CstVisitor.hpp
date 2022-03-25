#pragma once

#include "antlr4-runtime.h"
#include "OberonBaseVisitor.h"
#include "OberonParser.h"

class CstVisitor : public oberon::OberonBaseVisitor
{
  public:
    /* handles defining identifiers*/
    virtual antlrcpp::Any visitIdentifier(oberon::OberonParser::IdentifierContext* context) override;
    virtual antlrcpp::Any visitQualifiedIdentifier(oberon::OberonParser::QualifiedIdentifierContext* context) override;
    virtual antlrcpp::Any visitExportedIdentifier(oberon::OberonParser::ExportedIdentifierContext *context) override;
    virtual antlrcpp::Any visitNormalInteger(oberon::OberonParser::NormalIntegerContext *context) override;
    virtual antlrcpp::Any visitHexInteger(oberon::OberonParser::HexIntegerContext *context) override;
    virtual antlrcpp::Any visitRealNumber(oberon::OberonParser::RealNumberContext *context) override;
    virtual antlrcpp::Any visitScaleFactor(oberon::OberonParser::ScaleFactorContext *context) override;
    virtual antlrcpp::Any visitNumber(oberon::OberonParser::NumberContext *context) override;
    virtual antlrcpp::Any visitStringConstant(oberon::OberonParser::StringConstantContext *context) override;
    virtual antlrcpp::Any visitNumberStr(oberon::OberonParser::NumberStrContext *context) override;
    virtual antlrcpp::Any visitConstDeclaration(oberon::OberonParser::ConstDeclarationContext *context) override;
    virtual antlrcpp::Any visitConstExpression(oberon::OberonParser::ConstExpressionContext *context) override;
    virtual antlrcpp::Any visitTypeDeclaration(oberon::OberonParser::TypeDeclarationContext *context) override;
    virtual antlrcpp::Any visitType(oberon::OberonParser::TypeContext *context) override;
    virtual antlrcpp::Any visitArrayType(oberon::OberonParser::ArrayTypeContext *context) override;
    virtual antlrcpp::Any visitLength(oberon::OberonParser::LengthContext *context) override;
    virtual antlrcpp::Any visitRecordType(oberon::OberonParser::RecordTypeContext *context) override;
    virtual antlrcpp::Any visitBaseType(oberon::OberonParser::BaseTypeContext *context) override;
    virtual antlrcpp::Any visitFieldListSequence(oberon::OberonParser::FieldListSequenceContext *context) override;
    virtual antlrcpp::Any visitFieldList(oberon::OberonParser::FieldListContext *context) override;
    virtual antlrcpp::Any visitIdentList(oberon::OberonParser::IdentListContext *context) override;
    virtual antlrcpp::Any visitPointerType(oberon::OberonParser::PointerTypeContext *context) override;
    virtual antlrcpp::Any visitProcedureType(oberon::OberonParser::ProcedureTypeContext *context) override;
    virtual antlrcpp::Any visitVariableDeclaration(oberon::OberonParser::VariableDeclarationContext *context) override;
    virtual antlrcpp::Any visitExpression(oberon::OberonParser::ExpressionContext *context) override;
    virtual antlrcpp::Any visitRelation(oberon::OberonParser::RelationContext *context) override;
    virtual antlrcpp::Any visitSimpleExpression(oberon::OberonParser::SimpleExpressionContext *context) override;
    virtual antlrcpp::Any visitAddOperator(oberon::OberonParser::AddOperatorContext *context) override;
    virtual antlrcpp::Any visitTerm(oberon::OberonParser::TermContext *context) override;
    virtual antlrcpp::Any visitMulOperator(oberon::OberonParser::MulOperatorContext *context) override;
    virtual antlrcpp::Any visitFactor(oberon::OberonParser::FactorContext *context) override;
    virtual antlrcpp::Any visitDesignator(oberon::OberonParser::DesignatorContext *context) override;
    virtual antlrcpp::Any visitSelector(oberon::OberonParser::SelectorContext *context) override;
    virtual antlrcpp::Any visitSet(oberon::OberonParser::SetContext *context) override;
    virtual antlrcpp::Any visitElement(oberon::OberonParser::ElementContext *context) override;
    virtual antlrcpp::Any visitExpList(oberon::OberonParser::ExpListContext *context) override;
    virtual antlrcpp::Any visitActualParameters(oberon::OberonParser::ActualParametersContext *context) override;
    virtual antlrcpp::Any visitStatement(oberon::OberonParser::StatementContext *context) override;
    virtual antlrcpp::Any visitAssignment(oberon::OberonParser::AssignmentContext *context) override;
    virtual antlrcpp::Any visitProcedureCall(oberon::OberonParser::ProcedureCallContext *context) override;
    virtual antlrcpp::Any visitStatementSequence(oberon::OberonParser::StatementSequenceContext *context) override;
    virtual antlrcpp::Any visitIfStatement(oberon::OberonParser::IfStatementContext *context) override;
    virtual antlrcpp::Any visitCaseStatement(oberon::OberonParser::CaseStatementContext *context) override;
    virtual antlrcpp::Any visitCaseSequence(oberon::OberonParser::CaseSequenceContext *context) override;
    virtual antlrcpp::Any visitCaseLabelList(oberon::OberonParser::CaseLabelListContext *context) override;
    virtual antlrcpp::Any visitLabelRange(oberon::OberonParser::LabelRangeContext *context) override;
    virtual antlrcpp::Any visitLabel(oberon::OberonParser::LabelContext *context) override;
    virtual antlrcpp::Any visitWhileStatement(oberon::OberonParser::WhileStatementContext *context) override;
    virtual antlrcpp::Any visitRepeatStatement(oberon::OberonParser::RepeatStatementContext *context) override;
    virtual antlrcpp::Any visitForStatement(oberon::OberonParser::ForStatementContext *context) override;
    virtual antlrcpp::Any visitProcedureDeclaration(oberon::OberonParser::ProcedureDeclarationContext *context) override;
    virtual antlrcpp::Any visitProcedureHeading(oberon::OberonParser::ProcedureHeadingContext *context) override;
    virtual antlrcpp::Any visitProcedureBody(oberon::OberonParser::ProcedureBodyContext *context) override;
    virtual antlrcpp::Any visitDeclarationSequence(oberon::OberonParser::DeclarationSequenceContext *context) override;
    virtual antlrcpp::Any visitFormalParameters(oberon::OberonParser::FormalParametersContext *context) override;
    virtual antlrcpp::Any visitFpSection(oberon::OberonParser::FpSectionContext *context) override;
    virtual antlrcpp::Any visitFormalType(oberon::OberonParser::FormalTypeContext *context) override;
    virtual antlrcpp::Any visitModule(oberon::OberonParser::ModuleContext *context) override;
    virtual antlrcpp::Any visitImportList(oberon::OberonParser::ImportListContext *context) override;
    virtual antlrcpp::Any visitImportStatement(oberon::OberonParser::ImportStatementContext *context) override;
};

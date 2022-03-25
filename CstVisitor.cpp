#include "CstVisitor.hpp"

antlrcpp::Any CstVisitor::visitIdentifier(oberon::OberonParser::IdentifierContext* context) 
{ 
  return visitChildren(context); 
}

antlrcpp::Any CstVisitor::visitQualifiedIdentifier(oberon::OberonParser::QualifiedIdentifierContext* context) 
{ 
  return visitChildren(context); 
}

antlrcpp::Any CstVisitor::visitExportedIdentifier(oberon::OberonParser::ExportedIdentifierContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitNormalInteger(oberon::OberonParser::NormalIntegerContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitHexInteger(oberon::OberonParser::HexIntegerContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitRealNumber(oberon::OberonParser::RealNumberContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitScaleFactor(oberon::OberonParser::ScaleFactorContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitNumber(oberon::OberonParser::NumberContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitStringConstant(oberon::OberonParser::StringConstantContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitNumberStr(oberon::OberonParser::NumberStrContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitConstDeclaration(oberon::OberonParser::ConstDeclarationContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitConstExpression(oberon::OberonParser::ConstExpressionContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitTypeDeclaration(oberon::OberonParser::TypeDeclarationContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitType(oberon::OberonParser::TypeContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitArrayType(oberon::OberonParser::ArrayTypeContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitLength(oberon::OberonParser::LengthContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitRecordType(oberon::OberonParser::RecordTypeContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitBaseType(oberon::OberonParser::BaseTypeContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitFieldListSequence(oberon::OberonParser::FieldListSequenceContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitFieldList(oberon::OberonParser::FieldListContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitIdentList(oberon::OberonParser::IdentListContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitPointerType(oberon::OberonParser::PointerTypeContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitProcedureType(oberon::OberonParser::ProcedureTypeContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitVariableDeclaration(oberon::OberonParser::VariableDeclarationContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitExpression(oberon::OberonParser::ExpressionContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitRelation(oberon::OberonParser::RelationContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitSimpleExpression(oberon::OberonParser::SimpleExpressionContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitAddOperator(oberon::OberonParser::AddOperatorContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitTerm(oberon::OberonParser::TermContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitMulOperator(oberon::OberonParser::MulOperatorContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitFactor(oberon::OberonParser::FactorContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitDesignator(oberon::OberonParser::DesignatorContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitSelector(oberon::OberonParser::SelectorContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitSet(oberon::OberonParser::SetContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitElement(oberon::OberonParser::ElementContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitExpList(oberon::OberonParser::ExpListContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitActualParameters(oberon::OberonParser::ActualParametersContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitStatement(oberon::OberonParser::StatementContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitAssignment(oberon::OberonParser::AssignmentContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitProcedureCall(oberon::OberonParser::ProcedureCallContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitStatementSequence(oberon::OberonParser::StatementSequenceContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitIfStatement(oberon::OberonParser::IfStatementContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitCaseStatement(oberon::OberonParser::CaseStatementContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitCaseSequence(oberon::OberonParser::CaseSequenceContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitCaseLabelList(oberon::OberonParser::CaseLabelListContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitLabelRange(oberon::OberonParser::LabelRangeContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitLabel(oberon::OberonParser::LabelContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitWhileStatement(oberon::OberonParser::WhileStatementContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitRepeatStatement(oberon::OberonParser::RepeatStatementContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitForStatement(oberon::OberonParser::ForStatementContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitProcedureDeclaration(oberon::OberonParser::ProcedureDeclarationContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitProcedureHeading(oberon::OberonParser::ProcedureHeadingContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitProcedureBody(oberon::OberonParser::ProcedureBodyContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitDeclarationSequence(oberon::OberonParser::DeclarationSequenceContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitFormalParameters(oberon::OberonParser::FormalParametersContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitFpSection(oberon::OberonParser::FpSectionContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitFormalType(oberon::OberonParser::FormalTypeContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitModule(oberon::OberonParser::ModuleContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitImportList(oberon::OberonParser::ImportListContext *context) { return visitChildren(context); }
antlrcpp::Any CstVisitor::visitImportStatement(oberon::OberonParser::ImportStatementContext *context) { return visitChildren(context); }



#include "CstVisitor.hpp"
#include "ErrorReporter.hpp"
#include "Expr.hpp"

CstVisitor::CstVisitor(ErrorReporter *reporter)
{
}

ModuleExpr *CstVisitor::ast() const
{
  return m_tree.get();
}

antlrcpp::Any CstVisitor::visitExportedIdentifier(OberonParser::ExportedIdentifierContext *context)
{
  // add exported identifier to symbol table with the exported flag turned on.
  auto identifier = context->ID()->getText();
  auto val = context->getText();
  return visitChildren(context);
}

antlrcpp::Any CstVisitor::visitDecimalInteger(OberonParser::DecimalIntegerContext *context)
{
  return visitChildren(context);
}

antlrcpp::Any CstVisitor::visitHexInteger(OberonParser::HexIntegerContext *context)
{
  std::ostringstream oss;
  for (auto ch: context->HEXDIGIT()) {
    oss << ch->getText();
  }
  auto res = stoll(oss.str(), 0, 16);
  m_tree->addChild(new IntegerExpr(res));
  return visitChildren(context);
}

antlrcpp::Any CstVisitor::visitInteger(OberonParser::IntegerContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitReal(OberonParser::RealContext *ctx)
{
//    std::ostringstream oss;
//    for(auto ch : ctx->DIGIT()) {
//        oss << ch->getText();
//    }
//    auto val = std::stoll(oss.str(), 0);
//    val += std::stod(ctx->decimalPart()->getText(), 0);
//    if (!ctx->scaleFactor()->isEmpty()) {
//
//    }
//
//    m_tree->addChild(new RealNumberExpr(val));
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitString(OberonParser::StringContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitComment(OberonParser::CommentContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitIdentifierAssignment(OberonParser::IdentifierAssignmentContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitConstDeclaration(OberonParser::ConstDeclarationContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitRelation(OberonParser::RelationContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitParseAddOperator(OberonParser::ParseAddOperatorContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitMultOperator(OberonParser::MultOperatorContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitImprt(OberonParser::ImprtContext *ctx)
{
  return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitImportList(OberonParser::ImportListContext *ctx)
{
  return visitChildren(ctx);
}


antlrcpp::Any CstVisitor::visitModule(OberonParser::ModuleContext *context)
{
  m_tree.reset(new ModuleExpr{context->ID(0)->toString()});
  return visitChildren(context);
}


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

antlrcpp::Any CstVisitor::visitQualifiedIdentifier(OberonParser::QualifiedIdentifierContext *context)
{
  // check to see if particular module has been imported or not
  // if it has not yet been imported throw error.
    return visitChildren(context);
}

antlrcpp::Any CstVisitor::visitExportedIdentifier(OberonParser::ExportedIdentifierContext *context)
{
  // add exported identifier to symbol table with the exported flag turned on.
  return visitChildren(context);
}

antlrcpp::Any CstVisitor::visitInteger(OberonParser::IntegerContext *ctx) {
    auto parseHexInteger = [&ctx](auto const val) {
        auto res = std::stoi(ctx->DIGIT(0)->getText(), 0);
        std::ostringstream oss;
        for(auto ch : ctx->HEXDIGIT()) {
            oss << ch->getText();
        }
        res += stoll(oss.str(), 0, 16);
        return res;
    };
    auto val = ctx->HEXDIGIT().empty() ?
            std::stoll(ctx->getText()) :
            parseHexInteger(ctx);
    m_tree->addChild(new IntegerExpr(val));
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitReal(OberonParser::RealContext *ctx) {
    std::ostringstream oss;
    for(auto ch : ctx->DIGIT()) {
        oss << ch->getText();
    }
    auto val = std::stoll(oss.str(), 0);
    val += std::stod(ctx->decimalPart()->getText(), 0);
    if (!ctx->scaleFactor()->isEmpty()) {

    }

    m_tree->addChild(new RealNumberExpr(val));
    return visitChildren(ctx);
}

antlrcpp::Any CstVisitor::visitString(OberonParser::StringContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitComment(OberonParser::CommentContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitIdentifierAssignment(OberonParser::IdentifierAssignmentContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitConstDeclaration(OberonParser::ConstDeclarationContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitParseExpression(OberonParser::ParseExpressionContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitParseSimpleExpression(OberonParser::ParseSimpleExpressionContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitTerm(OberonParser::TermContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitFactor(OberonParser::FactorContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitRelation(OberonParser::RelationContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitParseAddOperator(OberonParser::ParseAddOperatorContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitMultOperator(OberonParser::MultOperatorContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitImprt(OberonParser::ImprtContext *ctx) {
    return visitChildren(ctx);
}
antlrcpp::Any CstVisitor::visitImportList(OberonParser::ImportListContext *ctx) {
    return visitChildren(ctx);
}


antlrcpp::Any CstVisitor::visitModule(OberonParser::ModuleContext *context)
{
    m_tree.reset(new ModuleExpr{context->ID(0)->toString()});
    return visitChildren(context);
}

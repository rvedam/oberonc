#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <stdexcept>

// -----------------------------------------------------------------------
// Oberon-07 LL(1) recursive-descent parser
//
// Each grammar production has a corresponding parse*() method.
// The parser maintains a one-token lookahead (`cur_`) which is always
// the *current* (not yet consumed) token.
//
// Error recovery: on a syntax error a ParseError exception is thrown
// with a human-readable message including file / line / column.
// -----------------------------------------------------------------------

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Parser {
public:
    explicit Parser(Lexer& lexer);

    // Parse a full compilation unit.
    Module parseModule();

private:
    Lexer&  lex_;
    Token   cur_;

    // ---- Token management ---------------------------------------------
    const Token& current() const { return cur_; }
    TokenKind    kind()    const { return cur_.kind; }

    // Consume and return the current token; advance to next.
    Token advance();

    // Consume token of expected kind, throw if mismatch.
    Token expect(TokenKind k);

    // Return true and consume if current token matches.
    bool match(TokenKind k);

    // Error helpers
    [[noreturn]] void error(const std::string& msg);
    [[noreturn]] void errorExpected(const std::string& what);

    // ---- Grammar productions ------------------------------------------
    // Lexical
    std::string parseIdent();
    std::pair<std::string,std::string> parseQualident();  // {module, ident}
    std::pair<std::string,bool>        parseIdentdef();   // {name, exported}

    // Types
    TypePtr            parseType();
    TypePtr            parseArrayType();
    TypePtr            parseRecordType();
    FieldList          parseFieldList();
    TypePtr            parsePointerType();
    TypePtr            parseProcedureType();
    FormalParameters   parseFormalParameters();
    FPSection          parseFPSection();
    TypePtr            parseFormalType();

    // Expressions
    ExprPtr  parseExpression();
    ExprPtr  parseSimpleExpression();
    ExprPtr  parseTerm();
    ExprPtr  parseFactor();
    SetExpr* parseSet();          // returns heap-allocated node
    DesignPtr parseDesignator();

    std::optional<std::vector<ExprPtr>> parseActualParameters();
    std::vector<ExprPtr>                parseExpList();

    // Statements
    StmtSeq parseStatementSequence();
    StmtPtr parseStatement();
    StmtPtr parseIfStatement();
    StmtPtr parseCaseStatement();
    StmtPtr parseWhileStatement();
    StmtPtr parseRepeatStatement();
    StmtPtr parseForStatement();

    // Case helpers
    CaseArm        parseCaseArm();
    CaseLabelRange parseLabelRange();
    ExprPtr        parseLabel();

    // Declarations
    DeclarationSequence parseDeclarationSequence();
    ConstDecl           parseConstDeclaration();
    TypeDecl            parseTypeDeclaration();
    VarDecl             parseVariableDeclaration();
    std::shared_ptr<ProcDecl> parseProcedureDeclaration();

    // Relation / operator helpers (return std::nullopt if not a relation)
    std::optional<BinaryOp> tryRelation();
    std::optional<BinaryOp> tryAddOperator();
    std::optional<BinaryOp> tryMulOperator();
};

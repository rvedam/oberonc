#include "parser.hpp"
#include <cassert>
#include <sstream>

// Simple formatting helper for compilers without <format>
template<typename... Args>
static std::string fmt(Args&&... args) {
    std::ostringstream ss;
    (ss << ... << args);
    return ss.str();
}

// -----------------------------------------------------------------------
// Constructor / token management
// -----------------------------------------------------------------------
Parser::Parser(Lexer& lexer) : lex_(lexer), cur_(lex_.next()) {}

Token Parser::advance() {
    Token t = cur_;
    cur_ = lex_.next();
    return t;
}

Token Parser::expect(TokenKind k) {
    if (cur_.kind != k)
        errorExpected(std::string("'") + tokenKindName(k) + "'");
    return advance();
}

bool Parser::match(TokenKind k) {
    if (cur_.kind != k) return false;
    advance();
    return true;
}

void Parser::error(const std::string& msg) {
    throw ParseError(fmt(lex_.filename(), ":", cur_.loc.line,
        ":", cur_.loc.column, ": ", msg));
}

void Parser::errorExpected(const std::string& what) {
    error(fmt("expected ", what, " but got '",
        (cur_.text.empty() ? tokenKindName(cur_.kind) : cur_.text), "'"));
}

// -----------------------------------------------------------------------
// Lexical helpers
// -----------------------------------------------------------------------

// ident = letter {letter | digit}
std::string Parser::parseIdent() {
    if (kind() != TokenKind::Ident)
        errorExpected("identifier");
    return advance().text;
}

// qualident = [ident "."] ident
// We return {module, name} where module is empty if unqualified.
std::pair<std::string,std::string> Parser::parseQualident() {
    std::string first = parseIdent();
    if (kind() == TokenKind::Dot) {
        advance();   // consume '.'
        std::string second = parseIdent();
        return {first, second};
    }
    return {"", first};
}

// identdef = ident ["*"]
std::pair<std::string,bool> Parser::parseIdentdef() {
    std::string name     = parseIdent();
    bool        exported = match(TokenKind::Star);
    return {name, exported};
}

// -----------------------------------------------------------------------
// Types
// -----------------------------------------------------------------------

// type = qualident | ArrayType | RecordType | PointerType | ProcedureType
TypePtr Parser::parseType() {
    SourceLoc l = cur_.loc;
    switch (kind()) {
    case TokenKind::Ident: {
        auto [mod, id] = parseQualident();
        auto n = std::make_unique<QualIdentType>();
        n->loc = l; n->module = mod; n->ident = id;
        return n;
    }
    case TokenKind::Array:     return parseArrayType();
    case TokenKind::Record:    return parseRecordType();
    case TokenKind::Pointer:   return parsePointerType();
    case TokenKind::Procedure: return parseProcedureType();
    default:
        errorExpected("type");
    }
}

// ArrayType = ARRAY length {"," length} OF type
// length    = ConstExpression  (= expression)
TypePtr Parser::parseArrayType() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::Array);
    auto node = std::make_unique<ArrayTypeExpr>();
    node->loc = l;
    node->lengths.push_back(parseExpression());
    while (match(TokenKind::Comma))
        node->lengths.push_back(parseExpression());
    expect(TokenKind::Of);
    node->elemType = parseType();
    return node;
}

// RecordType = RECORD ["(" BaseType ")"] [FieldListSequence] END
// BaseType = qualident
// FieldListSequence = FieldList {";" FieldList}
TypePtr Parser::parseRecordType() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::Record);
    auto node = std::make_unique<RecordTypeExpr>();
    node->loc = l;

    if (match(TokenKind::LParen)) {
        auto [mod, id] = parseQualident();
        node->baseModule = mod; node->baseIdent = id;
        expect(TokenKind::RParen);
    }

    // FieldListSequence: FieldList may be empty (ident starts a FieldList)
    while (kind() == TokenKind::Ident) {
        node->fields.push_back(parseFieldList());
        if (!match(TokenKind::Semicolon)) break;
    }

    expect(TokenKind::End);
    return node;
}

// FieldList = IdentList ":" type
// IdentList = identdef {"," identdef}
FieldList Parser::parseFieldList() {
    FieldList fl;
    fl.loc = cur_.loc;
    auto [name, exp] = parseIdentdef();
    fl.names.push_back(name);
    while (match(TokenKind::Comma)) {
        auto [n2, e2] = parseIdentdef();
        fl.names.push_back(n2);
    }
    expect(TokenKind::Colon);
    fl.type = parseType();
    return fl;
}

// PointerType = POINTER TO type
TypePtr Parser::parsePointerType() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::Pointer);
    expect(TokenKind::To);
    auto node = std::make_unique<PointerTypeExpr>();
    node->loc  = l;
    node->base = parseType();
    return node;
}

// ProcedureType = PROCEDURE [FormalParameters]
TypePtr Parser::parseProcedureType() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::Procedure);
    auto node = std::make_unique<ProcedureTypeExpr>();
    node->loc = l;
    if (kind() == TokenKind::LParen)
        node->params = parseFormalParameters();
    return node;
}

// FormalParameters = "(" [FPSection {";" FPSection}] ")" [":" qualident]
FormalParameters Parser::parseFormalParameters() {
    FormalParameters fp;
    fp.loc = cur_.loc;
    expect(TokenKind::LParen);
    if (kind() != TokenKind::RParen) {
        fp.sections.push_back(parseFPSection());
        while (match(TokenKind::Semicolon))
            fp.sections.push_back(parseFPSection());
    }
    expect(TokenKind::RParen);
    if (match(TokenKind::Colon)) {
        auto [mod, id] = parseQualident();
        fp.retModule = mod; fp.retIdent = id;
    }
    return fp;
}

// FPSection = [VAR] ident {"," ident} ":" FormalType
FPSection Parser::parseFPSection() {
    FPSection s;
    s.loc   = cur_.loc;
    s.isVar = match(TokenKind::Var);
    s.names.push_back(parseIdent());
    while (match(TokenKind::Comma))
        s.names.push_back(parseIdent());
    expect(TokenKind::Colon);
    s.formalType = parseFormalType();
    return s;
}

// FormalType = {ARRAY OF} qualident
TypePtr Parser::parseFormalType() {
    SourceLoc l = cur_.loc;
    if (kind() == TokenKind::Array) {
        expect(TokenKind::Array);
        expect(TokenKind::Of);
        auto node = std::make_unique<ArrayTypeExpr>();
        node->loc      = l;
        node->elemType = parseFormalType();
        return node;
    }
    auto [mod, id] = parseQualident();
    auto node = std::make_unique<QualIdentType>();
    node->loc = l; node->module = mod; node->ident = id;
    return node;
}

// -----------------------------------------------------------------------
// Expressions
// -----------------------------------------------------------------------

// expression = SimpleExpression [relation SimpleExpression]
ExprPtr Parser::parseExpression() {
    SourceLoc l   = cur_.loc;
    ExprPtr   lhs = parseSimpleExpression();
    auto      rel = tryRelation();
    if (!rel) return lhs;
    auto node   = std::make_unique<BinaryExpr>();
    node->loc   = l;
    node->op    = *rel;
    node->left  = std::move(lhs);
    node->right = parseSimpleExpression();
    return node;
}

// relation = "=" | "#" | "<" | "<=" | ">" | ">=" | IN | IS
std::optional<BinaryOp> Parser::tryRelation() {
    switch (kind()) {
    case TokenKind::Eq:      advance(); return BinaryOp::Eq;
    case TokenKind::Hash:    advance(); return BinaryOp::Neq;
    case TokenKind::Lt:      advance(); return BinaryOp::Lt;
    case TokenKind::Leq:     advance(); return BinaryOp::Leq;
    case TokenKind::Gt:      advance(); return BinaryOp::Gt;
    case TokenKind::Geq:     advance(); return BinaryOp::Geq;
    case TokenKind::In:      advance(); return BinaryOp::In;
    case TokenKind::Is:      advance(); return BinaryOp::Is;
    default:                           return std::nullopt;
    }
}

// SimpleExpression = ["+" | "-"] term {AddOperator term}
ExprPtr Parser::parseSimpleExpression() {
    SourceLoc l = cur_.loc;
    ExprPtr   lhs;

    // Optional leading sign
    if (kind() == TokenKind::Plus || kind() == TokenKind::Minus) {
        UnaryOp op = (kind() == TokenKind::Plus) ? UnaryOp::Plus : UnaryOp::Minus;
        advance();
        auto u   = std::make_unique<UnaryExpr>();
        u->loc   = l;
        u->op    = op;
        u->operand = parseTerm();
        lhs = std::move(u);
    } else {
        lhs = parseTerm();
    }

    while (auto op = tryAddOperator()) {
        auto node   = std::make_unique<BinaryExpr>();
        node->loc   = l;
        node->op    = *op;
        node->left  = std::move(lhs);
        node->right = parseTerm();
        lhs = std::move(node);
    }
    return lhs;
}

// AddOperator = "+" | "-" | OR
std::optional<BinaryOp> Parser::tryAddOperator() {
    switch (kind()) {
    case TokenKind::Plus:  advance(); return BinaryOp::Add;
    case TokenKind::Minus: advance(); return BinaryOp::Sub;
    case TokenKind::Or:    advance(); return BinaryOp::Or;
    default:                          return std::nullopt;
    }
}

// term = factor {MulOperator factor}
ExprPtr Parser::parseTerm() {
    SourceLoc l   = cur_.loc;
    ExprPtr   lhs = parseFactor();
    while (auto op = tryMulOperator()) {
        auto node   = std::make_unique<BinaryExpr>();
        node->loc   = l;
        node->op    = *op;
        node->left  = std::move(lhs);
        node->right = parseFactor();
        lhs = std::move(node);
    }
    return lhs;
}

// MulOperator = "*" | "/" | DIV | MOD | "&"
std::optional<BinaryOp> Parser::tryMulOperator() {
    switch (kind()) {
    case TokenKind::Star:  advance(); return BinaryOp::Mul;
    case TokenKind::Slash: advance(); return BinaryOp::Div;
    case TokenKind::Div:   advance(); return BinaryOp::IDiv;
    case TokenKind::Mod:   advance(); return BinaryOp::IMod;
    case TokenKind::Amp:   advance(); return BinaryOp::And;
    default:                          return std::nullopt;
    }
}

// factor = number | string | NIL | TRUE | FALSE | set
//        | designator [ActualParameters]
//        | "(" expression ")"
//        | "~" factor
ExprPtr Parser::parseFactor() {
    SourceLoc l = cur_.loc;

    switch (kind()) {
    case TokenKind::Integer: {
        auto n = std::make_unique<IntLitExpr>();
        n->loc = l; n->raw = advance().text;
        return n;
    }
    case TokenKind::Real: {
        auto n = std::make_unique<RealLitExpr>();
        n->loc = l; n->raw = advance().text;
        return n;
    }
    case TokenKind::String: {
        auto n = std::make_unique<StringLitExpr>();
        n->loc = l; n->raw = advance().text;
        return n;
    }
    case TokenKind::HexStr: {
        // $hh hh ...$ byte array literal — only valid as argument to SYSTEM.ADR
        auto n = std::make_unique<StringLitExpr>();
        n->loc = l; n->raw = "$" + advance().text + "$";
        return n;
    }
    case TokenKind::Nil: {
        advance();
        auto n = std::make_unique<NilExpr>(); n->loc = l; return n;
    }
    case TokenKind::True: {
        advance();
        auto n = std::make_unique<TrueExpr>(); n->loc = l; return n;
    }
    case TokenKind::False: {
        advance();
        auto n = std::make_unique<FalseExpr>(); n->loc = l; return n;
    }
    case TokenKind::LBrace: {
        // set = "{" [element {"," element}] "}"
        auto n  = std::make_unique<SetExpr>();
        n->loc  = l;
        advance();  // consume '{'
        if (kind() != TokenKind::RBrace) {
            auto parseElement = [&]() {
                SetElement e;
                e.lo = parseExpression();
                if (match(TokenKind::DotDot))
                    e.hi = parseExpression();
                return e;
            };
            n->elements.push_back(parseElement());
            while (match(TokenKind::Comma))
                n->elements.push_back(parseElement());
        }
        expect(TokenKind::RBrace);
        return n;
    }
    case TokenKind::LParen: {
        advance();  // consume '('
        ExprPtr e = parseExpression();
        expect(TokenKind::RParen);
        return e;
    }
    case TokenKind::Tilde: {
        advance();
        auto n = std::make_unique<UnaryExpr>();
        n->loc = l; n->op = UnaryOp::Not;
        n->operand = parseFactor();
        return n;
    }
    case TokenKind::Ident: {
        // designator [ActualParameters]
        // Also handles type guard selectors: "(qualident)" followed by more selectors.
        // Per Oberon-07, "(qualident)" is a designator selector when followed by
        // another selector (".", "[", "^", "(").  We detect this after the fact:
        // if ActualParameters contain exactly one plain qualident AND the next token
        // is a selector, fold it as TypeGuardSelector and continue parsing selectors.
        auto node   = std::make_unique<DesignatorExpr>();
        node->loc   = l;
        node->desig = parseDesignator();
        while (kind() == TokenKind::LParen) {
            auto args = parseActualParameters();
            bool isTypeGuard = false;
            if (args && args->size() == 1) {
                auto* argDE = dynamic_cast<const DesignatorExpr*>(args->at(0).get());
                if (argDE && !argDE->args && argDE->desig->selectors.empty()) {
                    TokenKind nk = kind();
                    if (nk == TokenKind::Dot || nk == TokenKind::LBracket ||
                        nk == TokenKind::Caret || nk == TokenKind::LParen) {
                        // Type guard: fold into the designator and continue.
                        node->desig->selectors.push_back(TypeGuardSelector{
                            argDE->desig->ident, argDE->desig->module,
                            argDE->desig->loc});
                        parsePostSelectors(*node->desig);
                        isTypeGuard = true;
                    }
                }
            }
            if (!isTypeGuard) {
                node->args = std::move(args);
                break;
            }
        }
        return node;
    }
    default:
        errorExpected("expression");
    }
}

// designator = qualident {selector}
// selector   = "." ident | "[" ExpList "]" | "^" | "(" qualident ")"
DesignPtr Parser::parseDesignator() {
    SourceLoc l = cur_.loc;
    auto d = std::make_unique<Designator>();
    d->loc = l;

    // qualident = [ident "."] ident
    // We look ahead: if ident followed by "." followed by ident → qualified
    std::string first = parseIdent();
    // LL(1) disambiguation: is this a module-qualified name or an ident with
    // a field selector?  In Oberon, a module prefix ident must be a known
    // import, but at parse time we cannot distinguish – so we treat
    // ident "." ident as a qualified name and later selectors handle further "."
    if (kind() == TokenKind::Dot && cur_.loc.line == l.line) {
        // peek one more: if what follows '.' is an ident, treat as qualident
        // We have only 1-token lookahead, so we tentatively consume '.' here
        // and check that the next token is an ident.
        Token dot = advance();   // consume '.'
        if (kind() == TokenKind::Ident) {
            d->module = first;
            d->ident  = parseIdent();
        } else {
            // Not a qualident – it was a field selector after all.
            // Re-interpret: ident is the name, and we add a pending field sel.
            // But we already consumed '.', so we just build it as a selector.
            d->module = "";
            d->ident  = first;
            // The field selector target is whatever follows (which is NOT an ident
            // here – that's a parse error).
            errorExpected("identifier after '.'");
        }
    } else {
        d->module = "";
        d->ident  = first;
    }

    // {selector}
    while (true) {
        if (kind() == TokenKind::Dot) {
            SourceLoc sl = cur_.loc;
            advance();
            std::string field = parseIdent();
            d->selectors.push_back(FieldSelector{field, sl});
        } else if (kind() == TokenKind::LBracket) {
            SourceLoc sl = cur_.loc;
            advance();
            auto list = parseExpList();
            expect(TokenKind::RBracket);
            // Multi-index shorthand: a[i, j] = a[i][j]
            for (auto& idx : list)
                d->selectors.push_back(IndexSelector{std::move(idx), sl});
        } else if (kind() == TokenKind::Caret) {
            SourceLoc sl = cur_.loc;
            advance();
            d->selectors.push_back(DerefSelector{sl});
        } else {
            // '(' is NOT consumed here; it belongs to the caller as either
            // ActualParameters (in parseFactor) or a type guard that the
            // semantic phase will resolve.  Stopping the selector loop on '('
            // keeps the parser LL(1) without requiring extra lookahead.
            break;
        }
    }
    return d;
}

// parsePostSelectors — append "." ident, "[" ExpList "]", and "^" selectors to d.
// Called after a type guard "(qualident)" has been folded into a designator;
// continues consuming selectors until a "(" or non-selector token is seen.
void Parser::parsePostSelectors(Designator& d) {
    while (true) {
        if (kind() == TokenKind::Dot) {
            SourceLoc sl = cur_.loc;
            advance();
            std::string field = parseIdent();
            d.selectors.push_back(FieldSelector{field, sl});
        } else if (kind() == TokenKind::LBracket) {
            SourceLoc sl = cur_.loc;
            advance();
            auto list = parseExpList();
            expect(TokenKind::RBracket);
            for (auto& idx : list)
                d.selectors.push_back(IndexSelector{std::move(idx), sl});
        } else if (kind() == TokenKind::Caret) {
            SourceLoc sl = cur_.loc;
            advance();
            d.selectors.push_back(DerefSelector{sl});
        } else {
            break;
        }
    }
}

// ActualParameters = "(" [ExpList] ")"
std::optional<std::vector<ExprPtr>> Parser::parseActualParameters() {
    expect(TokenKind::LParen);
    std::vector<ExprPtr> args;
    if (kind() != TokenKind::RParen)
        args = parseExpList();
    expect(TokenKind::RParen);
    return args;
}

// ExpList = expression {"," expression}
std::vector<ExprPtr> Parser::parseExpList() {
    std::vector<ExprPtr> list;
    list.push_back(parseExpression());
    while (match(TokenKind::Comma))
        list.push_back(parseExpression());
    return list;
}

// -----------------------------------------------------------------------
// Statements
// -----------------------------------------------------------------------

// StatementSequence = statement {";" statement}
StmtSeq Parser::parseStatementSequence() {
    StmtSeq seq;
    if (auto s = parseStatement()) seq.push_back(std::move(s));
    while (match(TokenKind::Semicolon)) {
        if (auto s = parseStatement()) seq.push_back(std::move(s));
    }
    return seq;
}

// statement = [assignment | ProcedureCall | IfStatement | CaseStatement |
//              WhileStatement | RepeatStatement | ForStatement]
//
// LL(1) note: assignment and ProcedureCall both start with an ident.
// We parse the designator first, then check for ":=" vs "(" vs neither.
StmtPtr Parser::parseStatement() {
    SourceLoc l = cur_.loc;

    switch (kind()) {
    case TokenKind::If:     return parseIfStatement();
    case TokenKind::Case:   return parseCaseStatement();
    case TokenKind::While:  return parseWhileStatement();
    case TokenKind::Repeat: return parseRepeatStatement();
    case TokenKind::For:    return parseForStatement();

    case TokenKind::Ident: {
        // Could be assignment or procedure call.
        // Also handles type guard selectors in assignment LHS: "v(T).field := expr".
        DesignPtr d = parseDesignator();

        // Resolve any type guard selectors "(qualident)" that are part of the
        // designator.  A "(qualident)" followed by a selector or ":=" is a type
        // guard (Oberon-07 designator selector), not ActualParameters.
        while (kind() == TokenKind::LParen) {
            auto args = parseActualParameters();
            bool promoted = false;
            if (args && args->size() == 1) {
                auto* argDE = dynamic_cast<const DesignatorExpr*>(args->at(0).get());
                if (argDE && !argDE->args && argDE->desig->selectors.empty()) {
                    TokenKind nk = kind();
                    if (nk == TokenKind::Dot || nk == TokenKind::LBracket ||
                        nk == TokenKind::Caret || nk == TokenKind::LParen ||
                        nk == TokenKind::Assign) {
                        // Type guard: fold into the designator.
                        d->selectors.push_back(TypeGuardSelector{
                            argDE->desig->ident, argDE->desig->module,
                            argDE->desig->loc});
                        if (nk != TokenKind::Assign)
                            parsePostSelectors(*d);
                        promoted = true;
                    }
                }
            }
            if (!promoted) {
                // Not a type guard: procedure call statement.
                auto s  = std::make_unique<ProcCallStmt>();
                s->loc  = l;
                s->desig = std::move(d);
                s->args  = std::move(args);
                return s;
            }
        }

        if (kind() == TokenKind::Assign) {
            // assignment = designator ":=" expression
            advance();
            auto s    = std::make_unique<AssignStmt>();
            s->loc    = l;
            s->target = std::move(d);
            s->value  = parseExpression();
            return s;
        } else {
            // ProcedureCall = designator [ActualParameters]
            auto s  = std::make_unique<ProcCallStmt>();
            s->loc  = l;
            s->desig = std::move(d);
            return s;
        }
    }

    default:
        // Empty statement is allowed
        return nullptr;
    }
}

// IfStatement = IF expression THEN StatementSequence
//               {ELSIF expression THEN StatementSequence}
//               [ELSE StatementSequence] END
StmtPtr Parser::parseIfStatement() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::If);
    auto node = std::make_unique<IfStmt>();
    node->loc = l;

    IfBranch first;
    first.cond = parseExpression();
    expect(TokenKind::Then);
    first.body = parseStatementSequence();
    node->branches.push_back(std::move(first));

    while (kind() == TokenKind::Elsif) {
        advance();
        IfBranch branch;
        branch.cond = parseExpression();
        expect(TokenKind::Then);
        branch.body = parseStatementSequence();
        node->branches.push_back(std::move(branch));
    }

    if (match(TokenKind::Else))
        node->elseBranch = parseStatementSequence();

    expect(TokenKind::End);
    return node;
}

// CaseStatement = CASE expression OF case {"|" case} END
// case          = [CaseLabelList ":" StatementSequence]
StmtPtr Parser::parseCaseStatement() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::Case);
    auto node  = std::make_unique<CaseStmt>();
    node->loc  = l;
    node->expr = parseExpression();
    expect(TokenKind::Of);

    // First case arm
    node->arms.push_back(parseCaseArm());
    while (match(TokenKind::Pipe))
        node->arms.push_back(parseCaseArm());

    expect(TokenKind::End);
    return node;
}

// case = [CaseLabelList ":" StatementSequence]
CaseArm Parser::parseCaseArm() {
    CaseArm arm;
    arm.loc = cur_.loc;
    // An arm may be empty (if followed immediately by '|' or 'END')
    if (kind() == TokenKind::Pipe || kind() == TokenKind::End)
        return arm;
    // CaseLabelList = LabelRange {"," LabelRange}
    arm.labels.push_back(parseLabelRange());
    while (match(TokenKind::Comma))
        arm.labels.push_back(parseLabelRange());
    expect(TokenKind::Colon);
    arm.body = parseStatementSequence();
    return arm;
}

// LabelRange = label [".." label]
CaseLabelRange Parser::parseLabelRange() {
    CaseLabelRange r;
    r.lo = parseLabel();
    if (match(TokenKind::DotDot))
        r.hi = parseLabel();
    return r;
}

// label = integer | string | qualident
ExprPtr Parser::parseLabel() {
    SourceLoc l = cur_.loc;
    if (kind() == TokenKind::Integer) {
        auto n = std::make_unique<IntLitExpr>();
        n->loc = l; n->raw = advance().text; return n;
    }
    if (kind() == TokenKind::String) {
        auto n = std::make_unique<StringLitExpr>();
        n->loc = l; n->raw = advance().text; return n;
    }
    if (kind() == TokenKind::Ident) {
        auto [mod, id] = parseQualident();
        auto d = std::make_unique<Designator>();
        d->loc = l; d->module = mod; d->ident = id;
        auto e = std::make_unique<DesignatorExpr>();
        e->loc = l; e->desig = std::move(d);
        return e;
    }
    errorExpected("case label (integer, string, or identifier)");
}

// WhileStatement = WHILE expression DO StatementSequence
//                  {ELSIF expression DO StatementSequence} END
StmtPtr Parser::parseWhileStatement() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::While);
    auto node = std::make_unique<WhileStmt>();
    node->loc = l;

    WhileBranch first;
    first.cond = parseExpression();
    expect(TokenKind::Do);
    first.body = parseStatementSequence();
    node->branches.push_back(std::move(first));

    while (kind() == TokenKind::Elsif) {
        advance();
        WhileBranch b;
        b.cond = parseExpression();
        expect(TokenKind::Do);
        b.body = parseStatementSequence();
        node->branches.push_back(std::move(b));
    }
    expect(TokenKind::End);
    return node;
}

// RepeatStatement = REPEAT StatementSequence UNTIL expression
StmtPtr Parser::parseRepeatStatement() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::Repeat);
    auto node   = std::make_unique<RepeatStmt>();
    node->loc   = l;
    node->body  = parseStatementSequence();
    expect(TokenKind::Until);
    node->until = parseExpression();
    return node;
}

// ForStatement = FOR ident ":=" expression TO expression
//                [BY ConstExpression] DO StatementSequence END
StmtPtr Parser::parseForStatement() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::For);
    auto node = std::make_unique<ForStmt>();
    node->loc = l;
    node->var = parseIdent();
    expect(TokenKind::Assign);
    node->from = parseExpression();
    expect(TokenKind::To);
    node->to   = parseExpression();
    if (match(TokenKind::By))
        node->by = parseExpression();
    expect(TokenKind::Do);
    node->body = parseStatementSequence();
    expect(TokenKind::End);
    return node;
}

// -----------------------------------------------------------------------
// Declarations
// -----------------------------------------------------------------------

// DeclarationSequence =
//   [CONST {ConstDeclaration ";"}]
//   [TYPE  {TypeDeclaration  ";"}]
//   [VAR   {VariableDeclaration ";"}]
//   {ProcedureDeclaration ";"}
DeclarationSequence Parser::parseDeclarationSequence() {
    DeclarationSequence ds;

    if (match(TokenKind::Const)) {
        while (kind() == TokenKind::Ident) {
            ds.consts.push_back(parseConstDeclaration());
            expect(TokenKind::Semicolon);
        }
    }
    if (match(TokenKind::Type)) {
        while (kind() == TokenKind::Ident) {
            ds.types.push_back(parseTypeDeclaration());
            expect(TokenKind::Semicolon);
        }
    }
    if (match(TokenKind::Var)) {
        while (kind() == TokenKind::Ident) {
            ds.vars.push_back(parseVariableDeclaration());
            expect(TokenKind::Semicolon);
        }
    }
    while (kind() == TokenKind::Procedure) {
        ds.procs.push_back(parseProcedureDeclaration());
        expect(TokenKind::Semicolon);
    }
    return ds;
}

// ConstDeclaration = identdef "=" ConstExpression
ConstDecl Parser::parseConstDeclaration() {
    ConstDecl c;
    c.loc = cur_.loc;
    auto [name, exp] = parseIdentdef();
    c.name     = name;
    c.exported = exp;
    expect(TokenKind::Eq);
    c.value = parseExpression();   // ConstExpression = expression
    return c;
}

// TypeDeclaration = identdef "=" type
TypeDecl Parser::parseTypeDeclaration() {
    TypeDecl t;
    t.loc = cur_.loc;
    auto [name, exp] = parseIdentdef();
    t.name     = name;
    t.exported = exp;
    expect(TokenKind::Eq);
    t.type = parseType();
    return t;
}

// VariableDeclaration = IdentList ":" type
VarDecl Parser::parseVariableDeclaration() {
    VarDecl v;
    v.loc = cur_.loc;
    auto [name, exp] = parseIdentdef();
    v.names.push_back(name);
    v.exported.push_back(exp);
    while (match(TokenKind::Comma)) {
        auto [n2, e2] = parseIdentdef();
        v.names.push_back(n2);
        v.exported.push_back(e2);
    }
    expect(TokenKind::Colon);
    v.type = parseType();
    return v;
}

// ProcedureDeclaration = ProcedureHeading ";" ProcedureBody ident
// ProcedureHeading     = PROCEDURE identdef [FormalParameters]
// ProcedureBody        = DeclarationSequence
//                        [BEGIN StatementSequence]
//                        [RETURN expression] END
std::shared_ptr<ProcDecl> Parser::parseProcedureDeclaration() {
    SourceLoc l = cur_.loc;
    expect(TokenKind::Procedure);
    auto node  = std::make_shared<ProcDecl>();
    node->loc  = l;
    auto [name, exp] = parseIdentdef();
    node->name     = name;
    node->exported = exp;
    if (kind() == TokenKind::LParen)
        node->params = parseFormalParameters();
    expect(TokenKind::Semicolon);

    // ProcedureBody
    auto ds    = parseDeclarationSequence();
    node->consts = std::move(ds.consts);
    node->types  = std::move(ds.types);
    node->vars   = std::move(ds.vars);
    node->procs  = std::move(ds.procs);

    if (match(TokenKind::Begin))
        node->body = parseStatementSequence();
    if (match(TokenKind::Return)) {
        node->returnExpr = parseExpression();
        match(TokenKind::Semicolon); // allow optional trailing ";"
    }

    expect(TokenKind::End);

    // Trailing ident must match procedure name
    std::string closing = parseIdent();
    if (closing != node->name)
        error(fmt("closing identifier '", closing,
            "' does not match procedure name '", node->name, "'"));
    return node;
}

// -----------------------------------------------------------------------
// Module
// -----------------------------------------------------------------------

// module = MODULE ident ";" [ImportList] DeclarationSequence
//          [BEGIN StatementSequence] END ident "."
// ImportList = IMPORT import {"," import} ";"
// import     = ident [":=" ident]
Module Parser::parseModule() {
    Module m;
    m.loc = cur_.loc;
    expect(TokenKind::Module);
    m.name = parseIdent();
    expect(TokenKind::Semicolon);

    // Accept one or more IMPORT sections (some Oberon dialects allow multiple)
    while (match(TokenKind::Import)) {
        auto parseImport = [&]() {
            ImportEntry ie;
            ie.loc  = cur_.loc;
            ie.name = parseIdent();
            if (match(TokenKind::Assign)) {
                ie.alias = ie.name;
                ie.name  = parseIdent();
            }
            return ie;
        };
        m.imports.push_back(parseImport());
        while (match(TokenKind::Comma))
            m.imports.push_back(parseImport());
        expect(TokenKind::Semicolon);
    }

    m.decls = parseDeclarationSequence();

    if (match(TokenKind::Begin))
        m.body = parseStatementSequence();

    expect(TokenKind::End);
    match(TokenKind::Module); // accept optional 'MODULE' in 'END MODULE Name.'

    std::string closing = parseIdent();
    if (closing != m.name)
        error(fmt("closing identifier '", closing,
            "' does not match module name '", m.name, "'"));
    expect(TokenKind::Dot);

    return m;
}

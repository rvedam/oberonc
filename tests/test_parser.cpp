#include <catch2/catch_test_macros.hpp>
#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"

// Helper: parse a complete module from a string
static Module parseModule(std::string_view src) {
    Lexer  lex(src, "<test>");
    Parser p(lex);
    return p.parseModule();
}

// Helper: parse must throw ParseError
static void expectParseError(std::string_view src) {
    REQUIRE_THROWS_AS(parseModule(src), ParseError);
}

// -----------------------------------------------------------------------
// Minimal well-formed module
// -----------------------------------------------------------------------
TEST_CASE("Parser: empty module body", "[parser]") {
    auto m = parseModule("MODULE M; END M.");
    CHECK(m.name == "M");
    CHECK(m.imports.empty());
    CHECK(m.decls.consts.empty());
    CHECK(m.decls.types.empty());
    CHECK(m.decls.vars.empty());
    CHECK(m.decls.procs.empty());
    CHECK(m.body.empty());
}

TEST_CASE("Parser: module name must match closing ident", "[parser]") {
    expectParseError("MODULE Foo; END Bar.");
}

TEST_CASE("Parser: module must end with dot", "[parser]") {
    expectParseError("MODULE M; END M");
}

// -----------------------------------------------------------------------
// Imports
// -----------------------------------------------------------------------
TEST_CASE("Parser: single import", "[parser]") {
    auto m = parseModule("MODULE M; IMPORT Out; END M.");
    REQUIRE(m.imports.size() == 1);
    CHECK(m.imports[0].name == "Out");
    CHECK(m.imports[0].alias.empty());
}

TEST_CASE("Parser: multiple imports", "[parser]") {
    auto m = parseModule("MODULE M; IMPORT In, Out, Files; END M.");
    REQUIRE(m.imports.size() == 3);
    CHECK(m.imports[0].name == "In");
    CHECK(m.imports[1].name == "Out");
    CHECK(m.imports[2].name == "Files");
}

TEST_CASE("Parser: import with alias", "[parser]") {
    auto m = parseModule("MODULE M; IMPORT O := Out; END M.");
    REQUIRE(m.imports.size() == 1);
    CHECK(m.imports[0].alias == "O");
    CHECK(m.imports[0].name  == "Out");
}

// -----------------------------------------------------------------------
// Constant declarations
// -----------------------------------------------------------------------
TEST_CASE("Parser: integer constant", "[parser]") {
    auto m = parseModule("MODULE M; CONST N = 42; END M.");
    REQUIRE(m.decls.consts.size() == 1);
    CHECK(m.decls.consts[0].name     == "N");
    CHECK(m.decls.consts[0].exported == false);
    auto* lit = dynamic_cast<IntLitExpr*>(m.decls.consts[0].value.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->raw == "42");
}

TEST_CASE("Parser: exported constant", "[parser]") {
    auto m = parseModule("MODULE M; CONST N* = 1; END M.");
    CHECK(m.decls.consts[0].exported == true);
}

TEST_CASE("Parser: multiple constants", "[parser]") {
    auto m = parseModule("MODULE M; CONST A = 1; B = 2; C = 3; END M.");
    CHECK(m.decls.consts.size() == 3);
}

TEST_CASE("Parser: string constant", "[parser]") {
    auto m = parseModule(R"(MODULE M; CONST S = "hello"; END M.)");
    REQUIRE(m.decls.consts.size() == 1);
    auto* lit = dynamic_cast<StringLitExpr*>(m.decls.consts[0].value.get());
    REQUIRE(lit != nullptr);
}

TEST_CASE("Parser: real constant", "[parser]") {
    auto m = parseModule("MODULE M; CONST Pi = 3.14; END M.");
    auto* lit = dynamic_cast<RealLitExpr*>(m.decls.consts[0].value.get());
    REQUIRE(lit != nullptr);
    CHECK(lit->raw == "3.14");
}

TEST_CASE("Parser: boolean constants", "[parser]") {
    auto m = parseModule("MODULE M; CONST T = TRUE; F = FALSE; END M.");
    REQUIRE(m.decls.consts.size() == 2);
    CHECK(dynamic_cast<TrueExpr*>(m.decls.consts[0].value.get())  != nullptr);
    CHECK(dynamic_cast<FalseExpr*>(m.decls.consts[1].value.get()) != nullptr);
}

// -----------------------------------------------------------------------
// Type declarations
// -----------------------------------------------------------------------
TEST_CASE("Parser: simple type alias", "[parser]") {
    auto m = parseModule("MODULE M; TYPE T = INTEGER; END M.");
    REQUIRE(m.decls.types.size() == 1);
    CHECK(m.decls.types[0].name == "T");
    auto* qt = dynamic_cast<QualIdentType*>(m.decls.types[0].type.get());
    REQUIRE(qt != nullptr);
    CHECK(qt->ident == "INTEGER");
}

TEST_CASE("Parser: array type", "[parser]") {
    auto m = parseModule("MODULE M; TYPE T = ARRAY 10 OF INTEGER; END M.");
    auto* at = dynamic_cast<ArrayTypeExpr*>(m.decls.types[0].type.get());
    REQUIRE(at != nullptr);
    CHECK(at->lengths.size() == 1);
    auto* qt = dynamic_cast<QualIdentType*>(at->elemType.get());
    REQUIRE(qt != nullptr);
    CHECK(qt->ident == "INTEGER");
}

TEST_CASE("Parser: multi-dimensional array type", "[parser]") {
    auto m = parseModule("MODULE M; TYPE T = ARRAY 4, 4 OF REAL; END M.");
    auto* at = dynamic_cast<ArrayTypeExpr*>(m.decls.types[0].type.get());
    REQUIRE(at != nullptr);
    CHECK(at->lengths.size() == 2);
}

TEST_CASE("Parser: pointer type", "[parser]") {
    auto m = parseModule("MODULE M; TYPE P = POINTER TO INTEGER; END M.");
    auto* pt = dynamic_cast<PointerTypeExpr*>(m.decls.types[0].type.get());
    REQUIRE(pt != nullptr);
}

TEST_CASE("Parser: record type with fields", "[parser]") {
    auto m = parseModule(
        "MODULE M; TYPE R = RECORD x, y: INTEGER; z: REAL END; END M.");
    auto* rt = dynamic_cast<RecordTypeExpr*>(m.decls.types[0].type.get());
    REQUIRE(rt != nullptr);
    REQUIRE(rt->fields.size() == 2);
    CHECK(rt->fields[0].names == std::vector<std::string>{"x", "y"});
    CHECK(rt->fields[1].names == std::vector<std::string>{"z"});
}

TEST_CASE("Parser: record with base type", "[parser]") {
    auto m = parseModule(
        "MODULE M; TYPE R = RECORD (Base) x: INTEGER END; END M.");
    auto* rt = dynamic_cast<RecordTypeExpr*>(m.decls.types[0].type.get());
    REQUIRE(rt != nullptr);
    CHECK(rt->baseIdent == "Base");
}

TEST_CASE("Parser: procedure type", "[parser]") {
    auto m = parseModule(
        "MODULE M; TYPE F = PROCEDURE (x: INTEGER): BOOLEAN; END M.");
    auto* pt = dynamic_cast<ProcedureTypeExpr*>(m.decls.types[0].type.get());
    REQUIRE(pt != nullptr);
    REQUIRE(pt->params.has_value());
    CHECK(pt->params->retIdent == "BOOLEAN");
    REQUIRE(pt->params->sections.size() == 1);
    CHECK(pt->params->sections[0].names == std::vector<std::string>{"x"});
}

// -----------------------------------------------------------------------
// Variable declarations
// -----------------------------------------------------------------------
TEST_CASE("Parser: single variable", "[parser]") {
    auto m = parseModule("MODULE M; VAR x: INTEGER; END M.");
    REQUIRE(m.decls.vars.size() == 1);
    CHECK(m.decls.vars[0].names == std::vector<std::string>{"x"});
}

TEST_CASE("Parser: multiple variables same type", "[parser]") {
    auto m = parseModule("MODULE M; VAR x, y, z: INTEGER; END M.");
    REQUIRE(m.decls.vars.size() == 1);
    CHECK(m.decls.vars[0].names.size() == 3);
}

TEST_CASE("Parser: exported variable", "[parser]") {
    auto m = parseModule("MODULE M; VAR x*: INTEGER; END M.");
    CHECK(m.decls.vars[0].exported[0] == true);
}

// -----------------------------------------------------------------------
// Procedure declarations
// -----------------------------------------------------------------------
TEST_CASE("Parser: empty procedure", "[parser]") {
    auto m = parseModule(
        "MODULE M; PROCEDURE P; END P; END M.");
    REQUIRE(m.decls.procs.size() == 1);
    CHECK(m.decls.procs[0]->name == "P");
    CHECK(m.decls.procs[0]->body.empty());
}

TEST_CASE("Parser: procedure closing ident must match", "[parser]") {
    expectParseError("MODULE M; PROCEDURE P; END Q; END M.");
}

TEST_CASE("Parser: procedure with parameters", "[parser]") {
    auto m = parseModule(
        "MODULE M; PROCEDURE Add(a, b: INTEGER): INTEGER; BEGIN RETURN a + b END Add; END M.");
    auto& p = *m.decls.procs[0];
    REQUIRE(p.params.has_value());
    CHECK(p.params->sections.size() == 1);
    CHECK(p.params->sections[0].names.size() == 2);
    CHECK(p.params->retIdent == "INTEGER");
    REQUIRE(p.returnExpr != nullptr);
}

TEST_CASE("Parser: VAR parameter", "[parser]") {
    auto m = parseModule(
        "MODULE M; PROCEDURE Inc(VAR x: INTEGER); BEGIN x := x + 1 END Inc; END M.");
    auto& p = *m.decls.procs[0];
    REQUIRE(p.params.has_value());
    CHECK(p.params->sections[0].isVar == true);
}

TEST_CASE("Parser: exported procedure", "[parser]") {
    auto m = parseModule(
        "MODULE M; PROCEDURE Hello*; END Hello; END M.");
    CHECK(m.decls.procs[0]->exported == true);
}

TEST_CASE("Parser: nested procedure", "[parser]") {
    auto m = parseModule(
        "MODULE M; "
        "PROCEDURE Outer; "
        "  PROCEDURE Inner; END Inner; "
        "END Outer; "
        "END M.");
    REQUIRE(m.decls.procs.size() == 1);
    CHECK(m.decls.procs[0]->name == "Outer");
    CHECK(m.decls.procs[0]->procs.size() == 1);
    CHECK(m.decls.procs[0]->procs[0]->name == "Inner");
}

// -----------------------------------------------------------------------
// Expressions
// -----------------------------------------------------------------------
TEST_CASE("Parser: arithmetic expression in constant", "[parser]") {
    auto m = parseModule("MODULE M; CONST N = 1 + 2 * 3; END M.");
    auto* bin = dynamic_cast<BinaryExpr*>(m.decls.consts[0].value.get());
    REQUIRE(bin != nullptr);
    CHECK(bin->op == BinaryOp::Add);
    // Right side should be 2 * 3
    auto* rhs = dynamic_cast<BinaryExpr*>(bin->right.get());
    REQUIRE(rhs != nullptr);
    CHECK(rhs->op == BinaryOp::Mul);
}

TEST_CASE("Parser: unary negation", "[parser]") {
    auto m = parseModule("MODULE M; CONST N = -5; END M.");
    auto* u = dynamic_cast<UnaryExpr*>(m.decls.consts[0].value.get());
    REQUIRE(u != nullptr);
    CHECK(u->op == UnaryOp::Minus);
}

TEST_CASE("Parser: boolean NOT", "[parser]") {
    auto m = parseModule("MODULE M; CONST B = ~TRUE; END M.");
    auto* u = dynamic_cast<UnaryExpr*>(m.decls.consts[0].value.get());
    REQUIRE(u != nullptr);
    CHECK(u->op == UnaryOp::Not);
}

TEST_CASE("Parser: relational operators", "[parser]") {
    auto check = [](std::string_view op, BinaryOp expected) {
        std::string src = std::string("MODULE M; CONST B = 1 ") + std::string(op) + " 2; END M.";
        auto m = parseModule(src);
        auto* bin = dynamic_cast<BinaryExpr*>(m.decls.consts[0].value.get());
        REQUIRE(bin != nullptr);
        CHECK(bin->op == expected);
    };
    check("=",  BinaryOp::Eq);
    check("#",  BinaryOp::Neq);
    check("<",  BinaryOp::Lt);
    check("<=", BinaryOp::Leq);
    check(">",  BinaryOp::Gt);
    check(">=", BinaryOp::Geq);
}

TEST_CASE("Parser: NIL literal", "[parser]") {
    auto m = parseModule("MODULE M; CONST N = NIL; END M.");
    CHECK(dynamic_cast<NilExpr*>(m.decls.consts[0].value.get()) != nullptr);
}

TEST_CASE("Parser: set expression", "[parser]") {
    auto m = parseModule("MODULE M; CONST S = {1, 2, 3}; END M.");
    auto* s = dynamic_cast<SetExpr*>(m.decls.consts[0].value.get());
    REQUIRE(s != nullptr);
    CHECK(s->elements.size() == 3);
    for (auto& e : s->elements) CHECK(e.hi == nullptr);
}

TEST_CASE("Parser: set range expression", "[parser]") {
    auto m = parseModule("MODULE M; CONST S = {0..7}; END M.");
    auto* s = dynamic_cast<SetExpr*>(m.decls.consts[0].value.get());
    REQUIRE(s != nullptr);
    REQUIRE(s->elements.size() == 1);
    CHECK(s->elements[0].hi != nullptr);
}

TEST_CASE("Parser: empty set", "[parser]") {
    auto m = parseModule("MODULE M; CONST S = {}; END M.");
    auto* s = dynamic_cast<SetExpr*>(m.decls.consts[0].value.get());
    REQUIRE(s != nullptr);
    CHECK(s->elements.empty());
}

// -----------------------------------------------------------------------
// Statements
// -----------------------------------------------------------------------
TEST_CASE("Parser: assignment statement", "[parser]") {
    auto m = parseModule("MODULE M; VAR x: INTEGER; BEGIN x := 5 END M.");
    REQUIRE(m.body.size() == 1);
    auto* a = dynamic_cast<AssignStmt*>(m.body[0].get());
    REQUIRE(a != nullptr);
    CHECK(a->target->ident == "x");
}

TEST_CASE("Parser: procedure call without args", "[parser]") {
    auto m = parseModule("MODULE M; PROCEDURE P; END P; BEGIN P END M.");
    REQUIRE(m.body.size() == 1);
    auto* c = dynamic_cast<ProcCallStmt*>(m.body[0].get());
    REQUIRE(c != nullptr);
    CHECK(c->desig->ident == "P");
    CHECK(!c->args.has_value());
}

TEST_CASE("Parser: procedure call with args", "[parser]") {
    auto m = parseModule(
        "MODULE M; IMPORT Out; BEGIN Out.Int(42, 0) END M.");
    REQUIRE(m.body.size() == 1);
    auto* c = dynamic_cast<ProcCallStmt*>(m.body[0].get());
    REQUIRE(c != nullptr);
    REQUIRE(c->args.has_value());
    CHECK(c->args->size() == 2);
}

TEST_CASE("Parser: IF statement", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR x: INTEGER; BEGIN IF x > 0 THEN x := 1 END END M.");
    auto* s = dynamic_cast<IfStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
    CHECK(s->branches.size() == 1);
    CHECK(!s->elseBranch.has_value());
}

TEST_CASE("Parser: IF-ELSIF-ELSE statement", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR x: INTEGER; "
        "BEGIN "
        "  IF x < 0 THEN x := -1 "
        "  ELSIF x > 0 THEN x := 1 "
        "  ELSE x := 0 "
        "  END "
        "END M.");
    auto* s = dynamic_cast<IfStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
    CHECK(s->branches.size() == 2);
    REQUIRE(s->elseBranch.has_value());
    CHECK(s->elseBranch->size() == 1);
}

TEST_CASE("Parser: WHILE statement", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR i: INTEGER; "
        "BEGIN WHILE i > 0 DO i := i - 1 END END M.");
    auto* s = dynamic_cast<WhileStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
    CHECK(s->branches.size() == 1);
}

TEST_CASE("Parser: WHILE with ELSIF", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR n: INTEGER; "
        "BEGIN "
        "  WHILE n > 5 DO n := n - 1 "
        "  ELSIF n > 0 DO n := 0 "
        "  END "
        "END M.");
    auto* s = dynamic_cast<WhileStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
    CHECK(s->branches.size() == 2);
}

TEST_CASE("Parser: REPEAT-UNTIL statement", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR n: INTEGER; "
        "BEGIN REPEAT n := n - 1 UNTIL n = 0 END M.");
    auto* s = dynamic_cast<RepeatStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
}

TEST_CASE("Parser: FOR statement", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR i: INTEGER; "
        "BEGIN FOR i := 0 TO 9 DO i := i END END M.");
    auto* s = dynamic_cast<ForStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
    CHECK(s->var == "i");
    CHECK(s->by == nullptr);
}

TEST_CASE("Parser: FOR statement with BY", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR i: INTEGER; "
        "BEGIN FOR i := 10 TO 0 BY -2 DO i := i END END M.");
    auto* s = dynamic_cast<ForStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
    CHECK(s->by != nullptr);
}

TEST_CASE("Parser: CASE statement", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR n: INTEGER; "
        "BEGIN "
        "  CASE n OF "
        "    1: n := 10 "
        "  | 2, 3: n := 20 "
        "  | 4..9: n := 30 "
        "  END "
        "END M.");
    auto* s = dynamic_cast<CaseStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
    CHECK(s->arms.size() == 3);
    // Second arm has two labels
    CHECK(s->arms[1].labels.size() == 2);
    // Third arm has a range
    REQUIRE(s->arms[2].labels.size() == 1);
    CHECK(s->arms[2].labels[0].hi != nullptr);
}

TEST_CASE("Parser: empty CASE arm", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR n: INTEGER; "
        "BEGIN CASE n OF 1: n := 1 | END END M.");
    auto* s = dynamic_cast<CaseStmt*>(m.body[0].get());
    REQUIRE(s != nullptr);
    CHECK(s->arms.size() == 2);
    CHECK(s->arms[1].labels.empty());
}

// -----------------------------------------------------------------------
// Designators
// -----------------------------------------------------------------------
TEST_CASE("Parser: qualified identifier", "[parser]") {
    auto m = parseModule(
        "MODULE M; IMPORT Out; BEGIN Out.Ln END M.");
    auto* c = dynamic_cast<ProcCallStmt*>(m.body[0].get());
    REQUIRE(c != nullptr);
    CHECK(c->desig->module == "Out");
    CHECK(c->desig->ident  == "Ln");
}

TEST_CASE("Parser: array index selector", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR a: ARRAY 5 OF INTEGER; "
        "BEGIN a[2] := 0 END M.");
    auto* a = dynamic_cast<AssignStmt*>(m.body[0].get());
    REQUIRE(a != nullptr);
    REQUIRE(!a->target->selectors.empty());
    CHECK(std::holds_alternative<IndexSelector>(a->target->selectors[0]));
}

TEST_CASE("Parser: pointer dereference selector", "[parser]") {
    auto m = parseModule(
        "MODULE M; "
        "TYPE Node = POINTER TO RECORD v: INTEGER END; "
        "VAR p: Node; "
        "BEGIN p^.v := 1 END M.");
    auto* a = dynamic_cast<AssignStmt*>(m.body[0].get());
    REQUIRE(a != nullptr);
    REQUIRE(a->target->selectors.size() == 2);
    CHECK(std::holds_alternative<DerefSelector>(a->target->selectors[0]));
    CHECK(std::holds_alternative<FieldSelector>(a->target->selectors[1]));
}

TEST_CASE("Parser: qualified ident r.x parsed as qualident (module=r, ident=x)", "[parser]") {
    // Per grammar: qualident = [ident "."] ident, so r.x is NOT a selector —
    // it's a two-part qualified name.  The semantic phase decides if r is a
    // module name or a record variable.
    auto m = parseModule(
        "MODULE M; "
        "TYPE R = RECORD x: INTEGER END; "
        "VAR r: R; "
        "BEGIN r.x := 0 END M.");
    auto* a = dynamic_cast<AssignStmt*>(m.body[0].get());
    REQUIRE(a != nullptr);
    CHECK(a->target->module == "r");
    CHECK(a->target->ident  == "x");
    CHECK(a->target->selectors.empty());
}

TEST_CASE("Parser: field selector via pointer dereference p^.x", "[parser]") {
    // p^.x is unambiguous: qualident=p, DerefSelector, FieldSelector(x)
    auto m = parseModule(
        "MODULE M; "
        "TYPE Node = POINTER TO RECORD x: INTEGER END; "
        "VAR p: Node; "
        "BEGIN p^.x := 7 END M.");
    auto* a = dynamic_cast<AssignStmt*>(m.body[0].get());
    REQUIRE(a != nullptr);
    CHECK(a->target->module.empty());
    CHECK(a->target->ident == "p");
    REQUIRE(a->target->selectors.size() == 2);
    CHECK(std::holds_alternative<DerefSelector>(a->target->selectors[0]));
    auto& sel = std::get<FieldSelector>(a->target->selectors[1]);
    CHECK(sel.ident == "x");
}

// -----------------------------------------------------------------------
// Statement sequences
// -----------------------------------------------------------------------
TEST_CASE("Parser: multiple statements separated by semicolons", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR x, y: INTEGER; "
        "BEGIN x := 1; y := 2; x := x + y END M.");
    CHECK(m.body.size() == 3);
}

TEST_CASE("Parser: trailing semicolon before END is allowed (empty stmt)", "[parser]") {
    auto m = parseModule(
        "MODULE M; VAR x: INTEGER; BEGIN x := 1; END M.");
    // The trailing ; creates a null (empty) statement which is filtered out
    CHECK(m.body.size() == 1);
}

// -----------------------------------------------------------------------
// Error cases
// -----------------------------------------------------------------------
TEST_CASE("Parser: missing END", "[parser]") {
    expectParseError("MODULE M; BEGIN ");
}

TEST_CASE("Parser: missing semicolon after const decl", "[parser]") {
    expectParseError("MODULE M; CONST N = 1 END M.");
}

TEST_CASE("Parser: missing := in assignment", "[parser]") {
    expectParseError("MODULE M; VAR x: INTEGER; BEGIN x 5 END M.");
}

TEST_CASE("Parser: malformed expression", "[parser]") {
    expectParseError("MODULE M; VAR x: INTEGER; BEGIN x := + END M.");
}

// -----------------------------------------------------------------------
// Cross-module import: a module that exports an Add function
// -----------------------------------------------------------------------

// The MathUtils module source used across the cross-module tests.
static constexpr std::string_view kMathUtils = R"(
MODULE MathUtils;

PROCEDURE Add*(a, b: INTEGER): INTEGER;
BEGIN
  RETURN a + b
END Add;

END MathUtils.
)";

TEST_CASE("Parser: MathUtils module with exported Add procedure", "[parser][import]") {
    auto m = parseModule(kMathUtils);
    CHECK(m.name == "MathUtils");
    CHECK(m.imports.empty());

    REQUIRE(m.decls.procs.size() == 1);
    const auto& add = *m.decls.procs[0];
    CHECK(add.name     == "Add");
    CHECK(add.exported == true);

    // Parameters: one section with (a, b: INTEGER)
    REQUIRE(add.params.has_value());
    const auto& fp = *add.params;
    REQUIRE(fp.sections.size() == 1);
    CHECK(fp.sections[0].names == std::vector<std::string>{"a", "b"});
    CHECK(fp.sections[0].isVar == false);
    auto* paramType = dynamic_cast<QualIdentType*>(fp.sections[0].formalType.get());
    REQUIRE(paramType != nullptr);
    CHECK(paramType->ident == "INTEGER");

    // Return type
    CHECK(fp.retIdent == "INTEGER");

    // Body: RETURN a + b
    REQUIRE(add.returnExpr != nullptr);
    auto* ret = dynamic_cast<BinaryExpr*>(add.returnExpr.get());
    REQUIRE(ret != nullptr);
    CHECK(ret->op == BinaryOp::Add);
}

TEST_CASE("Parser: module importing MathUtils and calling Add", "[parser][import]") {
    auto m = parseModule(R"(
MODULE Main;

IMPORT MathUtils;

VAR result: INTEGER;

BEGIN
  result := MathUtils.Add(3, 5)
END Main.
)");

    CHECK(m.name == "Main");

    // Import
    REQUIRE(m.imports.size() == 1);
    CHECK(m.imports[0].name  == "MathUtils");
    CHECK(m.imports[0].alias.empty());

    // VAR result: INTEGER
    REQUIRE(m.decls.vars.size() == 1);
    CHECK(m.decls.vars[0].names == std::vector<std::string>{"result"});

    // Body: result := MathUtils.Add(3, 5)
    REQUIRE(m.body.size() == 1);
    auto* assign = dynamic_cast<AssignStmt*>(m.body[0].get());
    REQUIRE(assign != nullptr);

    // LHS: result
    CHECK(assign->target->module.empty());
    CHECK(assign->target->ident == "result");

    // RHS: MathUtils.Add(3, 5) — a DesignatorExpr call
    auto* call = dynamic_cast<DesignatorExpr*>(assign->value.get());
    REQUIRE(call != nullptr);
    CHECK(call->desig->module == "MathUtils");
    CHECK(call->desig->ident  == "Add");
    REQUIRE(call->args.has_value());
    REQUIRE(call->args->size() == 2);

    // First argument: 3
    auto* arg0 = dynamic_cast<IntLitExpr*>((*call->args)[0].get());
    REQUIRE(arg0 != nullptr);
    CHECK(arg0->raw == "3");

    // Second argument: 5
    auto* arg1 = dynamic_cast<IntLitExpr*>((*call->args)[1].get());
    REQUIRE(arg1 != nullptr);
    CHECK(arg1->raw == "5");
}

TEST_CASE("Parser: module importing MathUtils with alias and calling Add", "[parser][import]") {
    auto m = parseModule(R"(
MODULE Main;

IMPORT Math := MathUtils;

VAR sum: INTEGER;

BEGIN
  sum := Math.Add(10, 20)
END Main.
)");

    // Import alias
    REQUIRE(m.imports.size() == 1);
    CHECK(m.imports[0].alias == "Math");
    CHECK(m.imports[0].name  == "MathUtils");

    // Body uses the alias as the module qualifier
    REQUIRE(m.body.size() == 1);
    auto* assign = dynamic_cast<AssignStmt*>(m.body[0].get());
    REQUIRE(assign != nullptr);

    auto* call = dynamic_cast<DesignatorExpr*>(assign->value.get());
    REQUIRE(call != nullptr);
    CHECK(call->desig->module == "Math");
    CHECK(call->desig->ident  == "Add");
    REQUIRE(call->args.has_value());
    CHECK(call->args->size() == 2);
}

#include <catch2/catch_test_macros.hpp>
#include "lexer.hpp"
#include <vector>

// Helper: lex all tokens from a string (excluding EOF)
static std::vector<Token> tokenize(std::string_view src) {
    Lexer lex(src, "<test>");
    std::vector<Token> toks;
    for (;;) {
        Token t = lex.next();
        if (t.is(TokenKind::Eof)) break;
        toks.push_back(t);
    }
    return toks;
}

static Token single(std::string_view src) {
    auto toks = tokenize(src);
    REQUIRE(toks.size() == 1);
    return toks[0];
}

// -----------------------------------------------------------------------
// Keywords
// -----------------------------------------------------------------------
TEST_CASE("Lexer: keywords are recognised", "[lexer]") {
    CHECK(single("MODULE").kind  == TokenKind::Module);
    CHECK(single("BEGIN").kind   == TokenKind::Begin);
    CHECK(single("END").kind     == TokenKind::End);
    CHECK(single("PROCEDURE").kind == TokenKind::Procedure);
    CHECK(single("VAR").kind     == TokenKind::Var);
    CHECK(single("CONST").kind   == TokenKind::Const);
    CHECK(single("TYPE").kind    == TokenKind::Type);
    CHECK(single("IMPORT").kind  == TokenKind::Import);
    CHECK(single("IF").kind      == TokenKind::If);
    CHECK(single("THEN").kind    == TokenKind::Then);
    CHECK(single("ELSE").kind    == TokenKind::Else);
    CHECK(single("ELSIF").kind   == TokenKind::Elsif);
    CHECK(single("WHILE").kind   == TokenKind::While);
    CHECK(single("DO").kind      == TokenKind::Do);
    CHECK(single("FOR").kind     == TokenKind::For);
    CHECK(single("TO").kind      == TokenKind::To);
    CHECK(single("BY").kind      == TokenKind::By);
    CHECK(single("REPEAT").kind  == TokenKind::Repeat);
    CHECK(single("UNTIL").kind   == TokenKind::Until);
    CHECK(single("CASE").kind    == TokenKind::Case);
    CHECK(single("OF").kind      == TokenKind::Of);
    CHECK(single("RETURN").kind  == TokenKind::Return);
    CHECK(single("ARRAY").kind   == TokenKind::Array);
    CHECK(single("RECORD").kind  == TokenKind::Record);
    CHECK(single("POINTER").kind == TokenKind::Pointer);
    CHECK(single("NIL").kind     == TokenKind::Nil);
    CHECK(single("TRUE").kind    == TokenKind::True);
    CHECK(single("FALSE").kind   == TokenKind::False);
    CHECK(single("IN").kind      == TokenKind::In);
    CHECK(single("IS").kind      == TokenKind::Is);
    CHECK(single("OR").kind      == TokenKind::Or);
    CHECK(single("DIV").kind     == TokenKind::Div);
    CHECK(single("MOD").kind     == TokenKind::Mod);
}

TEST_CASE("Lexer: keywords are case-sensitive (lowercase = ident)", "[lexer]") {
    CHECK(single("module").kind == TokenKind::Ident);
    CHECK(single("begin").kind  == TokenKind::Ident);
    CHECK(single("end").kind    == TokenKind::Ident);
}

// -----------------------------------------------------------------------
// Identifiers
// -----------------------------------------------------------------------
TEST_CASE("Lexer: identifiers", "[lexer]") {
    auto t = single("hello");
    CHECK(t.kind == TokenKind::Ident);
    CHECK(t.text == "hello");

    CHECK(single("x1").kind     == TokenKind::Ident);
    CHECK(single("MyVar").kind  == TokenKind::Ident);
    CHECK(single("A").kind      == TokenKind::Ident);
}

// -----------------------------------------------------------------------
// Integer literals
// -----------------------------------------------------------------------
TEST_CASE("Lexer: decimal integer literals", "[lexer]") {
    auto t = single("42");
    CHECK(t.kind == TokenKind::Integer);
    CHECK(t.text == "42");

    CHECK(single("0").kind  == TokenKind::Integer);
    CHECK(single("123").kind == TokenKind::Integer);
}

TEST_CASE("Lexer: hex integer literals end with H", "[lexer]") {
    auto t = single("0FFH");
    CHECK(t.kind == TokenKind::Integer);
    CHECK(t.text == "0FFH");

    CHECK(single("0H").kind  == TokenKind::Integer);
    CHECK(single("1AH").text == "1AH");
}

TEST_CASE("Lexer: character constant (hex with X)", "[lexer]") {
    auto t = single("0DX");
    CHECK(t.kind == TokenKind::String);
    CHECK(t.text == "0DX");
}

// -----------------------------------------------------------------------
// Real literals
// -----------------------------------------------------------------------
TEST_CASE("Lexer: real literals", "[lexer]") {
    CHECK(single("3.14").kind  == TokenKind::Real);
    CHECK(single("0.0").kind   == TokenKind::Real);
    CHECK(single("1.5E2").kind == TokenKind::Real);
    CHECK(single("2.0E+3").kind == TokenKind::Real);
    CHECK(single("9.9E-1").kind == TokenKind::Real);
    CHECK(single("3.14").text  == "3.14");
    CHECK(single("1.5E2").text == "1.5E2");
}

// -----------------------------------------------------------------------
// String literals
// -----------------------------------------------------------------------
TEST_CASE("Lexer: string literals", "[lexer]") {
    auto t = single(R"("hello")");
    CHECK(t.kind == TokenKind::String);
    CHECK(t.text == "\"hello\"");

    CHECK(single(R"("")").kind == TokenKind::String);
}

TEST_CASE("Lexer: unterminated string throws", "[lexer]") {
    Lexer lex("\"oops", "<test>");
    CHECK_THROWS_AS(lex.next(), std::runtime_error);
}

// -----------------------------------------------------------------------
// Operators and punctuation
// -----------------------------------------------------------------------
TEST_CASE("Lexer: single-character operators", "[lexer]") {
    CHECK(single("+").kind  == TokenKind::Plus);
    CHECK(single("-").kind  == TokenKind::Minus);
    CHECK(single("*").kind  == TokenKind::Star);
    CHECK(single("/").kind  == TokenKind::Slash);
    CHECK(single("~").kind  == TokenKind::Tilde);
    CHECK(single("&").kind  == TokenKind::Amp);
    CHECK(single(",").kind  == TokenKind::Comma);
    CHECK(single(";").kind  == TokenKind::Semicolon);
    CHECK(single("|").kind  == TokenKind::Pipe);
    CHECK(single("(").kind  == TokenKind::LParen);
    CHECK(single(")").kind  == TokenKind::RParen);
    CHECK(single("[").kind  == TokenKind::LBracket);
    CHECK(single("]").kind  == TokenKind::RBracket);
    CHECK(single("{").kind  == TokenKind::LBrace);
    CHECK(single("}").kind  == TokenKind::RBrace);
    CHECK(single("^").kind  == TokenKind::Caret);
    CHECK(single("=").kind  == TokenKind::Eq);
    CHECK(single("#").kind  == TokenKind::Hash);
    CHECK(single("<").kind  == TokenKind::Lt);
    CHECK(single(">").kind  == TokenKind::Gt);
}

TEST_CASE("Lexer: multi-character operators", "[lexer]") {
    CHECK(single("<=").kind == TokenKind::Leq);
    CHECK(single(">=").kind == TokenKind::Geq);
    CHECK(single(":=").kind == TokenKind::Assign);
    CHECK(single("..").kind == TokenKind::DotDot);
    CHECK(single(":").kind  == TokenKind::Colon);
    CHECK(single(".").kind  == TokenKind::Dot);
}

TEST_CASE("Lexer: < and <= are distinct", "[lexer]") {
    auto toks = tokenize("<= <");
    REQUIRE(toks.size() == 2);
    CHECK(toks[0].kind == TokenKind::Leq);
    CHECK(toks[1].kind == TokenKind::Lt);
}

TEST_CASE("Lexer: .. and . are distinct", "[lexer]") {
    auto toks = tokenize(".. .");
    REQUIRE(toks.size() == 2);
    CHECK(toks[0].kind == TokenKind::DotDot);
    CHECK(toks[1].kind == TokenKind::Dot);
}

// -----------------------------------------------------------------------
// Comments
// -----------------------------------------------------------------------
TEST_CASE("Lexer: block comments are skipped", "[lexer]") {
    auto toks = tokenize("(* comment *) x");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].kind == TokenKind::Ident);
    CHECK(toks[0].text == "x");
}

TEST_CASE("Lexer: nested comments are handled", "[lexer]") {
    auto toks = tokenize("(* outer (* inner *) still outer *) y");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].text == "y");
}

TEST_CASE("Lexer: unterminated comment throws", "[lexer]") {
    Lexer lex("(* oops", "<test>");
    CHECK_THROWS_AS(lex.next(), std::runtime_error);
}

TEST_CASE("Lexer: comment between tokens", "[lexer]") {
    auto toks = tokenize("a (* mid *) b");
    REQUIRE(toks.size() == 2);
    CHECK(toks[0].text == "a");
    CHECK(toks[1].text == "b");
}

// -----------------------------------------------------------------------
// Source locations
// -----------------------------------------------------------------------
TEST_CASE("Lexer: source locations are tracked", "[lexer]") {
    auto toks = tokenize("a\nb\n  c");
    REQUIRE(toks.size() == 3);
    CHECK(toks[0].loc.line == 1);
    CHECK(toks[0].loc.column == 1);
    CHECK(toks[1].loc.line == 2);
    CHECK(toks[2].loc.line == 3);
    CHECK(toks[2].loc.column == 3);
}

// -----------------------------------------------------------------------
// EOF
// -----------------------------------------------------------------------
TEST_CASE("Lexer: empty source gives EOF immediately", "[lexer]") {
    Lexer lex("", "<test>");
    CHECK(lex.next().kind == TokenKind::Eof);
    // Repeated calls should also give EOF
    CHECK(lex.next().kind == TokenKind::Eof);
}

TEST_CASE("Lexer: whitespace-only source gives EOF", "[lexer]") {
    Lexer lex("   \t\n  ", "<test>");
    CHECK(lex.next().kind == TokenKind::Eof);
}

// -----------------------------------------------------------------------
// Token sequence
// -----------------------------------------------------------------------
TEST_CASE("Lexer: module header tokens", "[lexer]") {
    auto toks = tokenize("MODULE Foo;");
    REQUIRE(toks.size() == 3);
    CHECK(toks[0].kind == TokenKind::Module);
    CHECK(toks[1].kind == TokenKind::Ident);
    CHECK(toks[1].text == "Foo");
    CHECK(toks[2].kind == TokenKind::Semicolon);
}

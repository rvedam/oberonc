#pragma once
#include <string>

// -----------------------------------------------------------------------
// Oberon-07 token kinds
// -----------------------------------------------------------------------
enum class TokenKind {
    // Literals
    Ident,
    Integer,   // decimal or 0..9{hexDigit}H
    Real,      // digit{digit}.{digit}[ScaleFactor]
    String,    // "..." or digit{hexDigit}X
    HexStr,    // $hh hh ...$ — Project Oberon inline byte array literal

    // Keywords (alphabetical)
    Array, Begin, By, Case, Const,
    Div, Do, Else, Elsif, End,
    False, For, If, Import, In,
    Is, Mod, Module, Nil, Of,
    Or, Pointer, Procedure, Record, Repeat,
    Return, Then, To, True, Type,
    Until, Var, While,

    // Operators and punctuation
    Plus,        // +
    Minus,       // -
    Star,        // *
    Slash,       // /
    Tilde,       // ~
    Amp,         // &
    Dot,         // .
    Comma,       // ,
    Semicolon,   // ;
    Pipe,        // |
    LParen,      // (
    RParen,      // )
    LBracket,    // [
    RBracket,    // ]
    LBrace,      // {
    RBrace,      // }
    Caret,       // ^
    Eq,          // =
    Hash,        // #
    Lt,          // <
    Gt,          // >
    Leq,         // <=
    Geq,         // >=
    DotDot,      // ..
    Colon,       // :
    Assign,      // :=

    Eof,
};

struct SourceLoc {
    int line   = 1;
    int column = 1;
};

struct Token {
    TokenKind   kind;
    std::string text;   // raw source text of the token
    SourceLoc   loc;

    [[nodiscard]] bool is(TokenKind k)    const { return kind == k; }
    [[nodiscard]] bool isNot(TokenKind k) const { return kind != k; }
};

// Human-readable name, useful in error messages
const char* tokenKindName(TokenKind k);

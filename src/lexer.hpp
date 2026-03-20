#pragma once
#include "token.hpp"
#include <string>
#include <string_view>

// -----------------------------------------------------------------------
// Oberon-07 lexer
//
// Produces one Token at a time from a UTF-8 source string.
// Call next() repeatedly; it returns TokenKind::Eof when exhausted.
//
// Oberon comments are (* ... *) and may be nested to arbitrary depth.
// -----------------------------------------------------------------------
class Lexer {
public:
    explicit Lexer(std::string_view source, std::string filename = "<input>");

    // Advance and return the next token.
    Token next();

    const std::string& filename() const { return filename_; }

private:
    std::string_view src_;
    std::string      filename_;
    std::size_t      pos_  = 0;
    int              line_ = 1;
    int              col_  = 1;

    // Low-level character access
    char  peek(int offset = 0) const;
    char  advance();
    bool  atEnd() const;

    // Skip whitespace and nested comments
    void skipWhitespaceAndComments();

    // Scan individual token categories
    Token scanIdent();          // letter {letter | digit}  → may be keyword
    Token scanNumber();         // integer or real
    Token scanString();         // "..." or digit{hexDigit}X
    Token makeToken(TokenKind k, std::string text, SourceLoc loc);

    // Current source location
    SourceLoc loc() const { return {line_, col_}; }
};

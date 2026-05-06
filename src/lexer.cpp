#include "lexer.hpp"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

// -----------------------------------------------------------------------
// Keyword table
// -----------------------------------------------------------------------
static const std::unordered_map<std::string_view, TokenKind> kKeywords = {
    {"ARRAY",     TokenKind::Array},
    {"BEGIN",     TokenKind::Begin},
    {"BY",        TokenKind::By},
    {"CASE",      TokenKind::Case},
    {"CONST",     TokenKind::Const},
    {"DIV",       TokenKind::Div},
    {"DO",        TokenKind::Do},
    {"ELSE",      TokenKind::Else},
    {"ELSIF",     TokenKind::Elsif},
    {"END",       TokenKind::End},
    {"FALSE",     TokenKind::False},
    {"FOR",       TokenKind::For},
    {"IF",        TokenKind::If},
    {"IMPORT",    TokenKind::Import},
    {"IN",        TokenKind::In},
    {"IS",        TokenKind::Is},
    {"MOD",       TokenKind::Mod},
    {"MODULE",    TokenKind::Module},
    {"NIL",       TokenKind::Nil},
    {"OF",        TokenKind::Of},
    {"OR",        TokenKind::Or},
    {"POINTER",   TokenKind::Pointer},
    {"PROCEDURE", TokenKind::Procedure},
    {"RECORD",    TokenKind::Record},
    {"REPEAT",    TokenKind::Repeat},
    {"RETURN",    TokenKind::Return},
    {"THEN",      TokenKind::Then},
    {"TO",        TokenKind::To},
    {"TRUE",      TokenKind::True},
    {"TYPE",      TokenKind::Type},
    {"UNTIL",     TokenKind::Until},
    {"VAR",       TokenKind::Var},
    {"WHILE",     TokenKind::While},
};

// -----------------------------------------------------------------------
// tokenKindName  (defined here to keep token.hpp header-only-like)
// -----------------------------------------------------------------------
const char* tokenKindName(TokenKind k) {
    switch (k) {
    case TokenKind::Ident:     return "identifier";
    case TokenKind::Integer:   return "integer literal";
    case TokenKind::Real:      return "real literal";
    case TokenKind::String:    return "string literal";
    case TokenKind::HexStr:    return "hex byte literal";
    case TokenKind::Array:     return "ARRAY";
    case TokenKind::Begin:     return "BEGIN";
    case TokenKind::By:        return "BY";
    case TokenKind::Case:      return "CASE";
    case TokenKind::Const:     return "CONST";
    case TokenKind::Div:       return "DIV";
    case TokenKind::Do:        return "DO";
    case TokenKind::Else:      return "ELSE";
    case TokenKind::Elsif:     return "ELSIF";
    case TokenKind::End:       return "END";
    case TokenKind::False:     return "FALSE";
    case TokenKind::For:       return "FOR";
    case TokenKind::If:        return "IF";
    case TokenKind::Import:    return "IMPORT";
    case TokenKind::In:        return "IN";
    case TokenKind::Is:        return "IS";
    case TokenKind::Mod:       return "MOD";
    case TokenKind::Module:    return "MODULE";
    case TokenKind::Nil:       return "NIL";
    case TokenKind::Of:        return "OF";
    case TokenKind::Or:        return "OR";
    case TokenKind::Pointer:   return "POINTER";
    case TokenKind::Procedure: return "PROCEDURE";
    case TokenKind::Record:    return "RECORD";
    case TokenKind::Repeat:    return "REPEAT";
    case TokenKind::Return:    return "RETURN";
    case TokenKind::Then:      return "THEN";
    case TokenKind::To:        return "TO";
    case TokenKind::True:      return "TRUE";
    case TokenKind::Type:      return "TYPE";
    case TokenKind::Until:     return "UNTIL";
    case TokenKind::Var:       return "VAR";
    case TokenKind::While:     return "WHILE";
    case TokenKind::Plus:      return "+";
    case TokenKind::Minus:     return "-";
    case TokenKind::Star:      return "*";
    case TokenKind::Slash:     return "/";
    case TokenKind::Tilde:     return "~";
    case TokenKind::Amp:       return "&";
    case TokenKind::Dot:       return ".";
    case TokenKind::Comma:     return ",";
    case TokenKind::Semicolon: return ";";
    case TokenKind::Pipe:      return "|";
    case TokenKind::LParen:    return "(";
    case TokenKind::RParen:    return ")";
    case TokenKind::LBracket:  return "[";
    case TokenKind::RBracket:  return "]";
    case TokenKind::LBrace:    return "{";
    case TokenKind::RBrace:    return "}";
    case TokenKind::Caret:     return "^";
    case TokenKind::Eq:        return "=";
    case TokenKind::Hash:      return "#";
    case TokenKind::Lt:        return "<";
    case TokenKind::Gt:        return ">";
    case TokenKind::Leq:       return "<=";
    case TokenKind::Geq:       return ">=";
    case TokenKind::DotDot:    return "..";
    case TokenKind::Colon:     return ":";
    case TokenKind::Assign:    return ":=";
    case TokenKind::Eof:       return "<eof>";
    }
    return "?";
}

// -----------------------------------------------------------------------
// Lexer implementation
// -----------------------------------------------------------------------
Lexer::Lexer(std::string_view source, std::string filename)
    : src_(source), filename_(std::move(filename)) {}

char Lexer::peek(int offset) const noexcept {
    std::size_t idx = pos_ + static_cast<std::size_t>(offset);
    return (idx < src_.size()) ? src_[idx] : '\0';
}

char Lexer::advance() noexcept {
    char c = src_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; }
    else           { ++col_; }
    return c;
}

bool Lexer::atEnd() const noexcept { return pos_ >= src_.size(); }

// Skip whitespace and nested Oberon comments (* ... *)
void Lexer::skipWhitespaceAndComments() {
    while (!atEnd()) {
        char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '(' && peek(1) == '*') {
            // Enter nested comment
            advance(); advance();          // consume (*
            int depth = 1;
            while (!atEnd() && depth > 0) {
                if (peek() == '(' && peek(1) == '*') {
                    advance(); advance(); ++depth;
                } else if (peek() == '*' && peek(1) == ')') {
                    advance(); advance(); --depth;
                } else {
                    advance();
                }
            }
            if (depth != 0) {
                throw std::runtime_error(
                    filename_ + ":" + std::to_string(line_) +
                    ": unterminated comment");
            }
        } else {
            break;
        }
    }
}

Token Lexer::makeToken(TokenKind k, std::string text, SourceLoc l) {
    return Token{k, std::move(text), l};
}

// ident = letter {letter | digit}
// After scanning, check keyword table.
Token Lexer::scanIdent() {
    SourceLoc l = loc();
    std::string text;
    while (!atEnd() &&
           (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        text += advance();
    }
    // Check keyword table
    if (auto it = kKeywords.find(text); it != kKeywords.end())
        return makeToken(it->second, text, l);
    return makeToken(TokenKind::Ident, text, l);
}

// Scans integer or real literals.
//   integer = digit {digit}
//           | digit {hexDigit} "H"
//   real    = digit {digit} "." {digit} [ScaleFactor]
//   ScaleFactor = "E" ["+" | "-"] digit {digit}
Token Lexer::scanNumber() {
    SourceLoc l = loc();
    std::string text;

    // Collect all hex-compatible digits
    while (!atEnd() && std::isxdigit(static_cast<unsigned char>(peek())))
        text += advance();

    // Hex integer: ends with 'H'
    if (!atEnd() && peek() == 'H') {
        text += advance();
        return makeToken(TokenKind::Integer, text, l);
    }

    // Real number: contains '.'
    if (!atEnd() && peek() == '.' && peek(1) != '.') {
        text += advance();   // consume '.'
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
            text += advance();
        // Optional ScaleFactor: E[+-]digit{digit}
        if (!atEnd() && peek() == 'E') {
            text += advance();
            if (!atEnd() && (peek() == '+' || peek() == '-'))
                text += advance();
            if (atEnd() || !std::isdigit(static_cast<unsigned char>(peek()))) {
                throw std::runtime_error(
                    filename_ + ":" + std::to_string(line_) +
                    ": expected digit after scale factor exponent");
            }
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
                text += advance();
        }
        return makeToken(TokenKind::Real, text, l);
    }

    return makeToken(TokenKind::Integer, text, l);
}

// string = '"' {character} '"'
//        | digit {hexDigit} "X"   (character code)
Token Lexer::scanString() {
    SourceLoc l = loc();
    std::string text;
    text += advance();  // consume opening '"'
    while (!atEnd() && peek() != '"') {
        if (peek() == '\n') {
            throw std::runtime_error(
                filename_ + ":" + std::to_string(line_) +
                ": unterminated string literal");
        }
        text += advance();
    }
    if (atEnd()) {
        throw std::runtime_error(
            filename_ + ":" + std::to_string(line_) +
            ": unterminated string literal");
    }
    text += advance();  // consume closing '"'
    return makeToken(TokenKind::String, text, l);
}

// Main tokenising entry point
Token Lexer::next() {
    skipWhitespaceAndComments();

    if (atEnd())
        return makeToken(TokenKind::Eof, "", loc());

    SourceLoc l   = loc();
    char      c   = peek();

    // Identifier or keyword
    if (std::isalpha(static_cast<unsigned char>(c)))
        return scanIdent();

    // Number literal (decimal or hex integer, real, or hex char constant)
    if (std::isdigit(static_cast<unsigned char>(c))) {
        // Peek ahead: if it's a hex-digit sequence followed by 'X' it's a char string.
        // We scan the number and let scanNumber differentiate.
        // But first handle the 'X' suffix (char constant) specially.
        // We collect digits/hexdigits then check the suffix.
        SourceLoc nl = loc();
        std::string text;
        while (!atEnd() && std::isxdigit(static_cast<unsigned char>(peek())))
            text += advance();
        // Character constant: digit{hexDigit}X
        if (!atEnd() && peek() == 'X') {
            text += advance();
            return makeToken(TokenKind::String, text, nl);
        }
        // Put the text back via a local re-scan is not possible; instead,
        // we handle the rest of number scanning inline here.
        // Hex integer
        if (!atEnd() && peek() == 'H') {
            text += advance();
            return makeToken(TokenKind::Integer, text, nl);
        }
        // Real
        if (!atEnd() && peek() == '.' && peek(1) != '.') {
            text += advance();
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
                text += advance();
            if (!atEnd() && peek() == 'E') {
                text += advance();
                if (!atEnd() && (peek() == '+' || peek() == '-'))
                    text += advance();
                while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
                    text += advance();
            }
            return makeToken(TokenKind::Real, text, nl);
        }
        return makeToken(TokenKind::Integer, text, nl);
    }

    // String literal
    if (c == '"')
        return scanString();

    // Hex byte array literal: $hh hh ...$ (Project Oberon extension)
    if (c == '$') {
        advance(); // consume opening '$'
        std::string text;
        while (!atEnd() && peek() != '$') {
            char ch = advance();
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
                text += ch;
        }
        if (atEnd())
            throw std::runtime_error(
                filename_ + ":" + std::to_string(l.line) + ":" +
                std::to_string(l.column) + ": unterminated hex byte literal");
        advance(); // consume closing '$'
        return makeToken(TokenKind::HexStr, text, l);
    }

    // Multi-character and single-character operators
    advance();   // consume c
    switch (c) {
    case '+': return makeToken(TokenKind::Plus,      "+", l);
    case '-': return makeToken(TokenKind::Minus,     "-", l);
    case '*': return makeToken(TokenKind::Star,      "*", l);
    case '/': return makeToken(TokenKind::Slash,     "/", l);
    case '~': return makeToken(TokenKind::Tilde,     "~", l);
    case '&': return makeToken(TokenKind::Amp,       "&", l);
    case ',': return makeToken(TokenKind::Comma,     ",", l);
    case ';': return makeToken(TokenKind::Semicolon, ";", l);
    case '|': return makeToken(TokenKind::Pipe,      "|", l);
    case '(': return makeToken(TokenKind::LParen,    "(", l);
    case ')': return makeToken(TokenKind::RParen,    ")", l);
    case '[': return makeToken(TokenKind::LBracket,  "[", l);
    case ']': return makeToken(TokenKind::RBracket,  "]", l);
    case '{': return makeToken(TokenKind::LBrace,    "{", l);
    case '}': return makeToken(TokenKind::RBrace,    "}", l);
    case '^': return makeToken(TokenKind::Caret,     "^", l);
    case '=': return makeToken(TokenKind::Eq,        "=", l);
    case '#': return makeToken(TokenKind::Hash,      "#", l);
    case '<':
        if (!atEnd() && peek() == '=') { advance(); return makeToken(TokenKind::Leq, "<=", l); }
        return makeToken(TokenKind::Lt, "<", l);
    case '>':
        if (!atEnd() && peek() == '=') { advance(); return makeToken(TokenKind::Geq, ">=", l); }
        return makeToken(TokenKind::Gt, ">", l);
    case ':':
        if (!atEnd() && peek() == '=') { advance(); return makeToken(TokenKind::Assign, ":=", l); }
        return makeToken(TokenKind::Colon, ":", l);
    case '.':
        if (!atEnd() && peek() == '.') { advance(); return makeToken(TokenKind::DotDot, "..", l); }
        return makeToken(TokenKind::Dot, ".", l);
    default:
        throw std::runtime_error(
            filename_ + ":" + std::to_string(l.line) +
            ":" + std::to_string(l.column) +
            ": unexpected character '" + c + "'");
    }
}

//
// Created by vedam on 12/18/2018.
//

#ifndef OBERONC_LEXER_H
#define OBERONC_LEXER_H

#include <utility>
#include <string>
#include <vector>
#include <unordered_map>

namespace lexer
{
/// TokenType tells us what tokens are available in our language
enum TokenType
{
    INTEGER = 0, /// integers
    FLOAT,       /// floating-point numbers (for purposes of simplicity, always double)
    WS,          /// whitespace
    LPAREN,      /// left parentheses
    RPAREN,      /// right parentheses
    LBRACK,      /// left brace
    RBRACK,      /// right brace
    IDENT,       /// for string identifiers (variables, functions, etc.)
    QUALIDENT,   /// for MODULE.function notation
    IDENTDEF,    /// For some kinds of procedural definitons
    SEMICOLON,   /// semi-colon
    COLON,       /// colon
    PERIOD,      /// period (useful for method definition)
    QUOT,        /// quotation
    NEWLINE,
    // keywords
    ARRAY,
    BEGIN,
    BY,
    CASE,
    CONST,
    DO,
    ELSE,
    ELSIF,
    END,
    FALSE,
    FOR,
    IF,
    IMPORT,
    IN,
    IS,
    MODULE,
    NIL,
    OF,
    OR,
    POINTER,
    PROCEDURE,
    RECORD,
    REPEAT,
    RETURN,
    THEN,
    TO,
    TRUE,
    TYPE,
    UNTIL,
    VAR,
    WHILE,
    PLUS,  /// addition operator
    MINUS, /// subtraction operator
    MULT,  /// multiplication operator
    DIV,   /// division operator
    MOD,   /// modulus operator
    UNKNOWN_TOKEN
};

struct TokenTypeHash
{
    template <typename T>
    std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};

const std::unordered_map<char, TokenType> opLookup({{'(', LPAREN},
                                                    {')', RPAREN},
                                                    {'+', PLUS},
                                                    {'-', MINUS},
                                                    {'*', MULT},
                                                    {'/', DIV},
                                                    {'.', PERIOD},
                                                    {';', SEMICOLON},
                                                    {':', COLON},
                                                    {'[', LBRACK},
                                                    {']', RBRACK},
                                                    {'\"', QUOT}});

const std::unordered_map<std::string, TokenType> keywordLookup({{"MOD", MOD},
                                                                {"ARRAY", ARRAY},
                                                                {"BY", BY},
                                                                {"CASE", CASE},
                                                                {"CONST", CONST},
                                                                {"DO", DO},
                                                                {"ELSE", ELSE},
                                                                {"ELSIF", ELSIF},
                                                                {"FALSE", FALSE},
                                                                {"FOR", FOR},
                                                                {"IF", IF},
                                                                {"IMPORT", IMPORT},
                                                                {"IN", IN},
                                                                {"IS", IS},
                                                                {"MODULE", MODULE},
                                                                {"NIL", NIL},
                                                                {"OF", OF},
                                                                {"OR", OR},
                                                                {"POINTER", POINTER},
                                                                {"PROCEDURE", PROCEDURE},
                                                                {"RECORD", RECORD},
                                                                {"REPEAT", REPEAT},
                                                                {"RETURN", RETURN},
                                                                {"THEN", THEN},
                                                                {"TO", TO},
                                                                {"TRUE", TRUE},
                                                                {"TYPE", TYPE},
                                                                {"UNTIL", UNTIL},
                                                                {"VAR", VAR},
                                                                {"WHILE", WHILE},
                                                                {"BEGIN", BEGIN},
                                                                {"END", END}});

const std::unordered_map<TokenType, std::string, TokenTypeHash> tokenTypeLookup({{WS, "WS"},
                                                                                 {LPAREN, "LPAREN"},
                                                                                 {RPAREN, "RPAREN"},
                                                                                 {PLUS, "PLUS"},
                                                                                 {MINUS, "MINUS"},
                                                                                 {MULT, "MULT"},
                                                                                 {DIV, "DIV"},
                                                                                 {PERIOD, "PERIOD"},
                                                                                 {SEMICOLON, "SEMICOLON"},
                                                                                 {COLON, "COLON"},
                                                                                 {LBRACK, "LBRACK"},
                                                                                 {RBRACK, "RBRACK"},
                                                                                 {QUOT, "QUOT"},
                                                                                 {IDENT, "IDENT"},
                                                                                 {IDENTDEF, "IDENTDEF"},
                                                                                 {QUALIDENT, "QUALIDENT"},
                                                                                 {MOD, "MOD"},
                                                                                 {ARRAY, "ARRAY"},
                                                                                 {BY, "BY"},
                                                                                 {CASE, "CASE"},
                                                                                 {CONST, "CONST"},
                                                                                 {DO, "DO"},
                                                                                 {ELSE, "ELSE"},
                                                                                 {ELSIF, "ELSIF"},
                                                                                 {FALSE, "FALSE"},
                                                                                 {FOR, "FOR"},
                                                                                 {IF, "IF"},
                                                                                 {IMPORT, "IMPORT"},
                                                                                 {IN, "IN"},
                                                                                 {IS, "IS"},
                                                                                 {MODULE, "MODULE"},
                                                                                 {NIL, "NIL"},
                                                                                 {OF, "OF"},
                                                                                 {OR, "OR"},
                                                                                 {POINTER, "POINTER"},
                                                                                 {PROCEDURE, "PROCEDURE"},
                                                                                 {RECORD, "RECORD"},
                                                                                 {REPEAT, "REPEAT"},
                                                                                 {RETURN, "RETURN"},
                                                                                 {THEN, "THEN"},
                                                                                 {TO, "TO"},
                                                                                 {TRUE, "TRUE"},
                                                                                 {TYPE, "TYPE"},
                                                                                 {UNTIL, "UNTIL"},
                                                                                 {VAR, "VAR"},
                                                                                 {WHILE, "WHILE"},
                                                                                 {BEGIN, "BEGIN"},
                                                                                 {END, "END"}});
} // namespace lexer

#endif //OBERONC_LEXER_H

//
// Created by vedam on 12/18/2018.
//

#pragma once

#include <utility>
#include <string>
#include <vector>
#include <unordered_map>

namespace lexer {
  /// TokenType tells us what tokens are available in our language
  enum TokenType {
    // Oberon Types
    INTEGER = 0, /// integers
    REAL,           /// floating-point numbers (for purposes of simplicity, always double)
    CHAR,           /// characters of the standard character set
    BOOLEAN,        /// boolean types
    SET,            /// sets of integers between 0 and implementation-dependent limit
    WS,             /// whitespace
    LPAREN,         /// left parentheses
    RPAREN,         /// right parentheses
    LBRACK,         /// left brace
    RBRACK,         /// right brace
    IDENT,          /// for string identifiers (variables, functions, etc.)
    QUALIDENT,      /// for MODULE.function notation
    IDENTDEF,       /// For some kinds of procedural definitons
    SEMICOLON,      /// semi-colon
    COLON,          /// colon
    PERIOD,         /// period (useful for method definition)
    QUOT,           /// quotation
    NEWLINE,
    // keywords
    ARRAY,
    BEGIN,
    BY,
    BYTE,
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
    PLUS,           /// addition operator
    MINUS,          /// subtraction operator
    MULT,           /// multiplication operator
    DIV,            /// division operator
    MOD,             /// modulus operator
    UNKNOWN_TOKEN
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

const std::unordered_map<std::string, TokenType> keywordLookup({
                                                                     {"INTEGER",   INTEGER},
                                                                     {"REAL",      REAL},
                                                                     {"BYTE",      BYTE},
                                                                     {"BOOLEAN",   BOOLEAN},
                                                                     {"CHAR",      CHAR},
                                                                     {"SET", SET},
                                                                     {"MOD",       MOD},
                                                                     {"ARRAY",     ARRAY},
                                                                     {"BY",        BY},
                                                                     {"BYTE",      BYTE},
                                                                     {"CASE",      CASE},
                                                                     {"CONST",     CONST},
                                                                     {"DO",        DO},
                                                                     {"ELSE",      ELSE},
                                                                     {"ELSIF",     ELSIF},
                                                                     {"FALSE",     FALSE},
                                                                     {"FOR",       FOR},
                                                                     {"IF",        IF},
                                                                     {"IMPORT",    IMPORT},
                                                                     {"IN",        IN},
                                                                     {"INTEGER",   INTEGER},
                                                                     {"IS",        IS},
                                                                     {"MODULE",    MODULE},
                                                                     {"NIL",       NIL},
                                                                     {"OF",        OF},
                                                                     {"OR",        OR},
                                                                     {"POINTER",   POINTER},
                                                                     {"PROCEDURE", PROCEDURE},
                                                                     {"REAL",      REAL},
                                                                     {"RECORD",    RECORD},
                                                                     {"REPEAT",    REPEAT},
                                                                     {"RETURN",    RETURN},
                                                                     {"THEN",      THEN},
                                                                     {"TO",        TO},
                                                                     {"TRUE",      TRUE},
                                                                     {"TYPE",      TYPE},
                                                                     {"UNTIL",     UNTIL},
                                                                     {"VAR",       VAR},
                                                                     {"WHILE",     WHILE},
                                                                     {"BEGIN",     BEGIN},
                                                                     {"END",       END}
                                                                 });

const std::unordered_map<TokenType, std::string> tokenTypeLookup({{WS, "WS"},
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
  std::string tokenTypeToString(TokenType type);
} // namespace lexer

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
        INTEGER = 0,    /// integers
        FLOAT,          /// floating-point numbers (for purposes of simplicity, always double)
        WS,             /// whitespace
        LPAREN,         /// left parentheses
        RPAREN,         /// right parentheses
        LBRACE,         /// left brace
        RBRACE,         /// right brace
        IDENT,          /// for string identifiers (variables, functions, etc.)
        QUALIDENT,      /// for MODULE.function notation
        IDENTDEF,       /// For some kinds of procedural definitons
        SEMICOLON,      /// semi-colon
        COLON,          /// colon
        PERIOD,
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
        PLUS,           /// addition operator
        MINUS,          /// subtraction operator
        MULT,           /// multiplication operator
        DIV,            /// division operator
        MOD,             /// modulus operator
        UNKNOWN_TOKEN
    };

    const std::unordered_map<std::string, TokenType> keywordLookup({
                                                                           {"(", LPAREN},
                                                                           {")", RPAREN},
                                                                           {"+", PLUS},
                                                                           {"-", MINUS},
                                                                           {"*", MULT},
                                                                           {"/", DIV},
                                                                           {".", PERIOD},
                                                                           {"MOD", MOD},
                                                                           {";", SEMICOLON},
                                                                           {":", COLON},
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
                                                                           {"OF",OF},
                                                                           {"OR",OR},
                                                                           {"POINTER",POINTER},
                                                                           {"PROCEDURE", PROCEDURE},
                                                                           {"RECORD",RECORD},
                                                                           {"REPEAT",REPEAT},
                                                                           {"RETURN",RETURN},
                                                                           {"THEN", THEN},
                                                                           {"TO", TO},
                                                                           {"TRUE", TRUE},
                                                                           {"TYPE", TYPE},
                                                                           {"UNTIL", UNTIL},
                                                                           {"VAR", VAR},
                                                                           {"WHILE", WHILE},
                                                                           {"BEGIN", BEGIN},
                                                                           {"END", END}
                                                                   });

}

#endif //OBERONC_LEXER_H

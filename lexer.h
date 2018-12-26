//
// Created by vedam on 12/18/2018.
//

#ifndef OBERONC_LEXER_H
#define OBERONC_LEXER_H

#include <utility>
#include <string>
#include <vector>

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
        KEYWORD,        /// Makes it easier to differentiate between keywords and identifiers
        IDENTIFIER      /// for string identifiers (variables, functions, etc.)
    };

    enum Keyword
    {
        // keywords
        ARRAY = 0,
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
        MOD             /// modulus operator
    };
}

#endif //OBERONC_LEXER_H

//
// Created by vedam on 12/25/2018.
//

#pragma once

#include <string>
#include "lexer_enums.h"

/**
 * Lexer package contains various components required to compose a stream of tokens
 * @package lexer
 */
namespace lexer
{
    /**
     * Generic Token class (determined by type of token)
     * @tparam T
     */
template<typename T>
class token
{
public:
    token(TokenType type, T value, int line, int cpos);
    virtual ~token() = default;
    TokenType type() const;
    T value() const;
    int line_number() const {return m_lineno;}
    int char_pos() const {return m_cpos;}
private:
    TokenType m_type;
    T m_value;
    int m_lineno;
    int m_cpos;
};
}

#include "token.t"


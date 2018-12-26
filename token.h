//
// Created by vedam on 12/25/2018.
//

#ifndef OBERONC_TOKEN_H
#define OBERONC_TOKEN_H

#include "lexer.h"

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
    token(TokenType type, T value);
    virtual ~token();
    TokenType type() const;
    T value() const;
private:
    TokenType m_type;
    T m_value;
};

}


#endif //OBERONC_TOKEN_H

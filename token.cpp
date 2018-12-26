//
// Created by vedam on 12/25/2018.
//

#include "token.h"

namespace lexer
{
    template<typename T>
    token<T>::token(TokenType type, T value) : m_type(type), m_value(value) {}

    template<typename T>
    token<T>::~token() {}

    template<typename T>
    TokenType token<T>::type() const {return m_type;}

    template<typename T>
    T token<T>::value() const {return m_value;}
}
//
// Created by Ramnarayan Vedam on 2/9/19.
//

#pragma once

#include <string>
#include <vector>
#include "token.h"

namespace lexer
{
    class scanner
    {
    public:
        scanner() = default;
        ~scanner() = default;
        void read(std::string fpath);
        std::vector<token<std::string>> tokens() const;
        std::vector<std::string> errors() const;
        void printTokens() const;
    private:
        std::vector<token<std::string>> m_tokens;
        std::vector<std::string> m_errors;
    };
}
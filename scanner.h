//
// Created by Ramnarayan Vedam on 2/9/19.
//

#pragma once

#include <string>
#include <vector>
#include "token.h"
#include "LogReporter.h"

namespace lexer
{
    class scanner
    {
    public:
        scanner(utils::LogReporter* log_reporter);

        ~scanner() = default;
        void read(std::string fpath);
        std::vector<token<std::string>> tokens() const;
        std::vector<std::string> errors() const;
        void printTokens() const;
    private:
       std::vector<token<std::string>> m_tokens;
       utils::LogReporter* m_log;
    };
}

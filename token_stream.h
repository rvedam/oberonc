//
// Created by vedam on 12/26/2018.
//

#ifndef OBERONC_TOKENSTREAM_H
#define OBERONC_TOKENSTREAM_H

#include <vector>
#include "lexer.h"
#include "token.h"

namespace lexer
{
    class token_stream
    {
    public:
        token_stream() = default;
        ~token_stream() = default;
        token_stream(const token_stream&) = default;
        token_stream(token_stream&&) = default;

        void scan(const std::string line);
        std::vector<TokenType> tokens() const;
        std::vector<std::string> values() const;
    private:
        std::vector<token<std::string>> m_tokens;
    };
}


#endif //OBERONC_TOKENSTREAM_H

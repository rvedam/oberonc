//
// Created by Ramnarayan Vedam on 2/9/19.
//

#ifndef OBERONC_SCANNER_H
#define OBERONC_SCANNER_H

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
    private:
        std::vector<token<std::string>> m_tokens;
        std::vector<std::string> m_errors;
    };
}


#endif //OBERONC_SCANNER_H

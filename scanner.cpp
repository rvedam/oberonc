//
// Created by Ramnarayan Vedam on 2/9/19.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "scanner.h"
#include "token.h"

namespace lexer
{
    void scanner::read(std::string fpath)
    {
        auto addToken = [this](lexer::TokenType type, std::string str, int line, int cpos) {
            token<std::string> tk(type, str, line, cpos);
            m_tokens.emplace_back(tk);
        };
        auto lookupKeywordToken = [](std::string str) -> lexer::TokenType {
            try {
                return keywordLookup.at(str);
            } catch(std::out_of_range e) {
                return lexer::UNKNOWN_TOKEN;
            }
        };
        auto lookupOpToken = [](char tok) -> lexer::TokenType  {
            try {
                return opLookup.at(tok);
            }catch (std::out_of_range e) {
                return lexer::UNKNOWN_TOKEN;
            }
        };
        std::ifstream ifs;
        ifs.open(fpath, std::ios::in);
        if (!ifs.is_open())
        {
            std::cout << "Filename not open" << std::endl;
            return;
        }
        int lineno = 1;
        int cpos = 1;
        while(!ifs.eof())
        {
            char ch;
            std::ostringstream tok;
            std::string token;
            if(isalpha(ifs.peek())) {
                while (isalpha(ifs.peek()) || isdigit(ifs.peek()) || ifs.peek() == '.' || ifs.peek() == '*') {
                    ifs.get(ch);
                    tok << ch;
                    ++cpos;
                }
                token = tok.str();
                auto ttype = lookupKeywordToken(tok.str());
                if(ttype == lexer::UNKNOWN_TOKEN) {
                    if(tok.str().find('.') == std::string::npos && tok.str().find('*') == std::string::npos) {
                        addToken(lexer::IDENT, tok.str(), lineno, cpos);
                    } else {
                        if(tok.str().find('.') != std::string::npos) {
                            addToken(lexer::QUALIDENT, tok.str(), lineno, cpos);
                        } else if(tok.str().find('*') != std::string::npos) {
                            addToken(lexer::IDENTDEF, tok.str(), lineno, cpos);
                        }
                    }
                } else {
                    addToken(ttype, tok.str(), lineno, cpos);
                }
            } else if(isspace(ifs.peek())) {
                ifs.get(ch);
                tok << ch;
                addToken(lexer::WS, tok.str(), lineno, cpos);
                ++cpos;
            } else if(isdigit(ifs.peek())) {
                while(isdigit(ifs.peek()) || ifs.peek() == '.') {
                    ifs.get(ch);
                    tok << ch;
                    ++cpos;
                }
                if(tok.str().find('.') == std::string::npos) {
                    addToken(lexer::INTEGER, tok.str(), lineno, cpos);
                } else {
                    addToken(lexer::FLOAT, tok.str(), lineno, cpos);
                }
            } else {
                char ch;
                ifs.get(ch);
                auto tokenType = lookupOpToken(ch);
                if(tokenType == lexer::UNKNOWN_TOKEN) {
                    std::cout << fpath << ":" << lineno << ":" << cpos << ": unknown token found." << std::endl;
                }
                ++cpos;
            }
            token = tok.str();
            tok.clear();
        }
        ifs.close();
    }

    std::vector<token<std::string>> scanner::tokens() const
    {
        return m_tokens;
    }

    std::vector<std::string> scanner::errors() const
    {
        return m_errors;
    }

    void scanner::printTokens() const
    {
        std::cout << "[";
        for(auto tok : m_tokens) {
            std::cout << "(" << tok.type() << ", " << tok.value() << ")";
        }
        std::cout << "]" << std::endl;
    }
}

//
// Created by vedam on 7/29/23.
//

#pragma once

#include <ostream>
#include <string>
#include "TokenType.hpp"

class Token
{
private:
  TokenType m_type;
  std::string m_str;
  std::string filename;
  int line;
  int column;
public:
  Token() = delete;
  explicit Token(TokenType type, const std::string str, const std::string filename, int line, int column);

  Token(std::initializer_list<Token>){}
  TokenType getTokenType() const;
  const std::string &getParsedString() const;
  const std::string &getFilename() const;
  int getLine() const;
  int getColumn() const;

  friend std::ostream &operator<<(std::ostream &os, const Token &token);
};

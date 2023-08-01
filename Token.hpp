//
// Created by vedam on 7/29/23.
//

#pragma once

#include <ostream>
#include "TokenType.hpp"

class Token
{
private:
  TokenType m_type;
  std::string m_str;
  std::string filename;
  size_t line;
  size_t column;
public:
  explicit Token(TokenType type, std::string str, std::string filename, size_t line, size_t column);
  TokenType getTokenType() const;
  const std::string &getParsedString() const;
  const std::string &getFilename() const;
  size_t getLine() const;
  size_t getColumn() const;

  friend std::ostream &operator<<(std::ostream &os, const Token &token);
};

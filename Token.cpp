//
// Created by vedam on 7/29/23.
//

#include "Token.hpp"

TokenType Token::getTokenType() const
{
  return m_type;
}

const std::string &Token::getParsedString() const
{
  return m_str;
}

const std::string &Token::getFilename() const
{
  return filename;
}

size_t Token::getLine() const
{
  return line;
}

size_t Token::getColumn() const
{
  return column;
}

std::ostream &operator<<(std::ostream &os, const Token &token)
{
  os << "<type: " << token.m_type << " string: " << token.m_str << " filename: " << token.filename << " line: "
     << token.line << " column: " << token.column << ">";
  return os;
}

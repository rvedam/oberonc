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

int Token::getLine() const
{
  return line;
}

int Token::getColumn() const
{
  return column;
}

std::ostream &operator<<(std::ostream &os, const Token &token)
{
  os << "<type: " << token.m_type << " string: " << token.m_str << " filename: " << token.filename << " line: "
     << token.line << " column: " << token.column << ">";
  return os;
}

Token::Token(TokenType mType, const std::string mStr, const std::string filename, int line, int column) : m_type(
    mType), m_str(mStr), filename(filename), line(line), column(column) {}

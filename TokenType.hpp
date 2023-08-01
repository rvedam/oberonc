//
// Created by vedam on 7/8/23.
//

#pragma once

#include <iostream>
#include <optional>

enum class TokenType
{
  // KEYWORDS
  DOUBLE_QUOTE,
  SINGLE_QUOTE ,
  LPAREN,
  RPAREN,
  LBRACK,
  RBRACK,
  LBRACE,
  RBRACE,
  PLUS,
  MINUS,
  TIMES,
  DIVIDE,
  TILDE,
  BITWISEAND,
  PERIOD,
  COMMA,
  SEMICOLON,
  PIPE,
  ASSIGN,
  POW,
  EQUAL,
  HASH,
  LESS_THAN,
  GREATER_THAN,
  LEQ,
  GEQ,
  ELLIPSIS,
  COLON,
  ARRAY,
  BEGIN,
  BY,
  CASE,
  CONST,
  DIV,
  DO,
  ELSE,
  ELSIF,
  END,
  FALSE,
  FOR,
  IF,
  IMPORT,
  IN,
  IS,
  MOD,
  MODULE,
  NIL,
  OF,
  OR,
  POINTER,
  PROCEDURE,
  RECORD,
  REPEAT,
  RETURN,
  THEN,
  TO,
  TRUE,
  TYPE,
  UNTIL,
  VAR,
  WHILE,
  // Identifiers
  IDENTIFIER,
  QUALIFIED_IDENTIFIER,
  INTEGER,
  REAL,
  UNEQUAL,
};

std::ostream& operator<<(std::ostream & str, const TokenType& type);
std::optional<TokenType> getTokenType(std::string tok);

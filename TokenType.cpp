//
// Created by vedam on 7/8/23.
//

#include <unordered_map>
#include <algorithm>
#include "TokenType.hpp"

std::unordered_map<std::string, TokenType> tokToTokenType = {
    // KEYWORDS
    {"\"", TokenType::DOUBLE_QUOTE},
    {"\'", TokenType::SINGLE_QUOTE },
    {"(", TokenType::LPAREN},
    {")", TokenType::RPAREN},
    {"[", TokenType::LBRACK},
    {"]", TokenType::RBRACK},
    {"{", TokenType::LBRACE},
    {"}", TokenType::RBRACE},
    {"+", TokenType::PLUS},
    {"-", TokenType::MINUS},
    {"*", TokenType::TIMES},
    {"/", TokenType::DIVIDE},
    {"~", TokenType::TILDE},
    {"&", TokenType::BITWISEAND},
    {".", TokenType::PERIOD},
    {",", TokenType::COMMA},
    {";", TokenType::SEMICOLON},
    {"|", TokenType::PIPE},
    {":=", TokenType::ASSIGN},
    {"^", TokenType::POW},
    {"=", TokenType::EQUAL},
    {"#", TokenType::UNEQUAL},
    {"#", TokenType::HASH},
    {"<", TokenType::LESS_THAN},
    {">", TokenType::GREATER_THAN},
    {"<=", TokenType::LEQ},
    {">=", TokenType::GEQ},
    {"...", TokenType::ELLIPSIS},
    {":", TokenType::COLON},
    {"ARRAY", TokenType::ARRAY},
    {"BEGIN", TokenType::BEGIN},
    {"BY", TokenType::BY},
    {"CASE", TokenType::CASE},
    {"CONST", TokenType::CONST},
    {"DIV", TokenType::DIV},
    {"DO", TokenType::DO},
    {"ELSE", TokenType::ELSE},
    {"ELSIF", TokenType::ELSIF},
    {"END", TokenType::END},
    {"FALSE", TokenType::FALSE},
    {"FOR", TokenType::FOR},
    {"IF", TokenType::IF},
    {"IMPORT", TokenType::IMPORT},
    {"IN", TokenType::IN},
    {"IS", TokenType::IS},
    {"MOD", TokenType::MOD},
    {"MODULE", TokenType::MODULE},
    {"NIL", TokenType::NIL},
    {"OF", TokenType::OF},
    {"OR", TokenType::OR},
    {"POINTER", TokenType::POINTER},
    {"PROCEDURE", TokenType::PROCEDURE},
    {"RECORD", TokenType::RECORD},
    {"REPEAT", TokenType::REPEAT},
    {"RETURN", TokenType::RETURN},
    {"THEN", TokenType::THEN},
    {"TO", TokenType::TO},
    {"TRUE", TokenType::TRUE},
    {"TYPE", TokenType::TYPE},
    {"UNTIL", TokenType::UNTIL},
    {"VAR", TokenType::VAR},
    {"WHILE", TokenType::WHILE}
};

std::ostream& operator<<(std::ostream & str, const TokenType& type) {
  switch(type) {
    case TokenType::IDENTIFIER:
      str << "IDENTIFIER";
      break;
    case TokenType::QUALIFIED_IDENTIFIER:
      str << "QUALIFIED_IDENTIFIER";
      break;
    case TokenType::INTEGER:
      str << "INTEGER";
      break;
    case TokenType::REAL:
      str << "REAL";
      break;
    case TokenType::EQUAL:
      str << "EQUAL";
      break;
    case TokenType::PERIOD:
      str << "PERIOD";
      break;
    case TokenType::MOD:
      str << "MOD";
      break;
    default:
      auto it = std::find_if(tokToTokenType.cbegin(), tokToTokenType.cend(),
                             [type](auto it) { return it.second == type; });
      if (it != tokToTokenType.end())
        str << it->first;
      break;
  }
  return str;
}

std::optional<TokenType> getTokenType(std::string tok){
  if(tokToTokenType.find(tok) != tokToTokenType.end())
    return std::make_optional(tokToTokenType[tok]);
  return std::optional<TokenType>();
}

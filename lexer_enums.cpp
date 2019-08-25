//
// Created by Ramnarayan Vedam on 2019-08-25.
//

#include "lexer_enums.h"
#include <sstream>

namespace lexer
{
  std::string tokenTypeToString(TokenType tokenType)
  {
    auto opStringPair = std::find_if(opLookup.cbegin(), opLookup.cend(), [tokenType](auto it) { return it.second == tokenType; });
    if(opStringPair != opLookup.cend())
    {
      std::ostringstream oss;
      oss << opStringPair->first;
      return oss.str();
    }
    auto tokenTypeStringPair = std::find_if(keywordLookup.cbegin(), keywordLookup.cend(),
                                            [tokenType](auto it) { return it.second == tokenType; });
    if (tokenTypeStringPair != keywordLookup.cend())
      return tokenTypeStringPair->first;
    else
    {
      switch(tokenType) {
        case QUALIDENT:
          return "QUALIDENT";
        case IDENT:
          return "IDENT";
        case WS:
          return "WS";
        case IDENTDEF:
          return "IDENTDEF";
        case INTEGER:
          return "INTEGER";
        case REAL:
          return "REAL";
        case CHAR:
          return "CHAR";
        case BOOLEAN:
          return "BOOLEAN";
        case LPAREN:
          return "LPAREN";
        case RPAREN:
          return "RPAREN";
        case LBRACK:
          return "LBRACK";
        case RBRACK:
          return "RBRACK";
        default:
          break;
      }
    }
    return "";
  }
}

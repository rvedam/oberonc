//
// Created by vedam on 7/8/23.
//

#pragma once

#include <string>
#include <utility>
#include <deque>
#include <fstream>
#include "TokenType.hpp"
#include "Token.hpp"

using TokenStream = std::deque<Token>;

class Tokenizer
{
private:
  TokenStream tokens;   // stream of scanned tokens.
  std::ifstream ifs;    // stream representing the file getting scanned.
  std::string filename; // name of file getting scanned.
  size_t line;          // line number in the file
  size_t pos;           // position in the line
public:
  // TODO: probably will want to create a way to handle multiple files (will require lazily parsing the individual files)
  /**
   * @brief constructor
   * @param filename - filename to scan
   */
  explicit Tokenizer(std::string filename);
  void scan();
  Token peek() const;
  Token getNextToken();
  void printTokens();
};

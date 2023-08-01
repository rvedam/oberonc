//
// Created by vedam on 7/8/23.
//

#include <sstream>
#include <regex>
#include "Tokenizer.hpp"

void Tokenizer::scan() {
  std::istreambuf_iterator<char> charIter(ifs), e;
  std::regex specialChar("[*/+-;(){}\"\',=.]", std::regex_constants::ECMAScript);
  while (charIter != e) {
    std::ostringstream oss;
    std::ostringstream cmpStrm;
    cmpStrm << *charIter;
    if (std::isspace(*charIter)) {
      charIter++;
      pos++;
      if(*charIter == '\n')
        line++;
    } else if (std::regex_match(cmpStrm.str(), specialChar) || *charIter == '[' || *charIter == ']') {
      oss << *charIter;
      auto tokenType = getTokenType(oss.str());
      tokens.emplace_back(Token{tokenType.value(), oss.str(), filename, line, pos});
      charIter++;
    } else if (*charIter == ':') {
      oss << *charIter;
      if (ifs.peek() == '=') {
        charIter++;
        oss << *charIter;
      }
      auto tokenType = getTokenType(oss.str());
      tokens.emplace_back(Token{tokenType.value(), oss.str(), filename, line, pos});
      charIter++;
    } else if (std::isdigit(*charIter)) {
      while (std::isdigit(*charIter)) {
        oss << *charIter;
        charIter++;
      }
      tokens.emplace_back(Token{TokenType::INTEGER, oss.str(), filename, line, pos});
    } else if (std::isalpha(*charIter)) {
      while (std::isalnum(*charIter) || *charIter == '_' || *charIter == '!') {
        oss << *charIter;
        charIter++;
      }
      auto token = oss.str();
      if (auto tokenType = getTokenType(oss.str()); tokenType.has_value()) {
        tokens.emplace_back(Token{tokenType.value(), oss.str(), filename, line, pos});
      }
      else
        tokens.emplace_back(Token{TokenType::IDENTIFIER, oss.str(), filename, line, pos});
    }
  }
}

void Tokenizer::printTokens() {
  for(auto& t : tokens) {
    std::cout << t << std::endl;
  }
}

Tokenizer::Tokenizer(std::string filename) :
  ifs(filename),
  filename(filename),
  line(1),
  pos(0)
{

}

Token Tokenizer::peek() const
{
  return tokens.front();
}

Token Tokenizer::getNextToken()
{
  auto res = tokens.front();
  tokens.pop_front();
  return res;
}

//
// Created by Ramnarayan Vedam on 2019-09-02.
//

#include "parser.h"
#include "scanner.h"

namespace oberon
{
  parser::parser(lexer::scanner* scanner, utils::LogReporter *logger) : m_scanner(scanner), m_log(logger)
  {}

  void parser::parse(std::string const &fname)
  {
    // 1. scan the file to generate the token stream
    m_scanner->read(fname);

    // 2. run the internal parsing algorithm to generate the parse tree
    parse_token_stream();
  }

  expr_ast const* parser::parse_tree() const
  {
    return m_tree.get();
  }

  void parser::parse_token_stream()
  {

  }
}
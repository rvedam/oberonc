//
// Created by Ramnarayan Vedam on 2019-09-02.
//

#include <iostream>
#include <sstream>
#include "parser.h"
#include "scanner.h"
#include "module_expr_ast.h"
#include "integer_expr_ast.h"
#include "double_expr_ast.h"
#include "string_expr_ast.h"

namespace oberon
{
  parser::parser(lexer::scanner* scanner, utils::LogReporter *logger) : m_scanner(scanner), m_log(logger)
  {}

  void parser::parse(std::string const &fname)
  {
    // 1. scan the file to generate the token stream
    m_scanner->read(fname);

    // 2. grab the start of the token stream (needed for our parsing algorithm)
    m_it = m_scanner->tokens().begin();

    // 3. run the internal parsing algorithm to generate the parse tree
    m_tree.reset(parse_token_stream(fname));
  }

  expr_ast const* parser::parse_tree() const
  {
    return m_tree.get();
  }

  expr_ast * parser::parse_token_stream(const std::string& fname)
  {
    while (m_it->type() != lexer::END && m_it != m_scanner->tokens().end())
    {
      switch(m_it->type())
      {
        case lexer::MODULE:
          return parse_module(fname);
        case lexer::PROCEDURE:
          return parse_procedure(fname);
        case lexer::CONST:
          return parse_const_declaration(fname);
        case lexer::QUOT:
          return parse_string(fname);
        default:
          std::cout << "UNKNOWN token" << std::endl;
          break;
      }
      m_it++;
    }
    return nullptr;
  }

  expr_ast * parser::parse_module(std::string const &fname)
  {
    std::cout << "parsing module" << std::endl;
    auto node = new module_expr_ast(m_it->value());
    if(m_it != m_scanner->tokens().end()) m_it++;
    while(m_it != m_scanner->tokens().end())
    {
      auto child_node = parse_token_stream(fname);
      if(!child_node && m_it->type() == lexer::END) {
        break;
      }
    }
    parse_end_expression(fname, lexer::MOD);
  }
  expr_ast * parser::parse_procedure(std::string const &fname)
  {
    std::cout << "parsing procedure" << std::endl;
    m_it++;
    parse_token_stream(fname);
    parse_end_expression(fname, lexer::PROCEDURE);
  }

  void parser::parse_end_expression(const std::string &fname, lexer::TokenType type)
  {
    std::cout << "looking up identifier for " << lexer::tokenTypeToString(type) << std::endl;
    // look up if the identifier matches the
    std::cout << "Okay looks like we're good " << std::endl;
    if(m_it != m_scanner->tokens().end()) m_it++;
  }

  expr_ast * parser::parse_const_declaration(const std::string &fname)
  {
    if(m_it != m_scanner->tokens().end()) m_it++;
  }

  expr_ast * parser::parse_string(const std::string &fname)
  {
    std::cout << "parsing string" << std::endl;
    if(m_it != m_scanner->tokens().end()) m_it++;
    std::ostringstream oss;
    for(;m_it->type() != lexer::QUOT; m_it++)
    {
      if(m_it->type() == lexer::WS)
        oss << " ";
      else
        oss << m_it->value();
    }
    // create new string constant
    return new string_expr_ast(oss.str());
  }
}
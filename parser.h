//
// Created by Ramnarayan Vedam on 2019-09-02.
//

#ifndef OBERONC_PARSER_H
#define OBERONC_PARSER_H

#include <memory>
#include "expr_ast.h"
#include "scanner.h"
#include "token.h"
#include "LogReporter.h"

namespace oberon
{
  class parser
  {
  public:
    parser(lexer::scanner* scanner, utils::LogReporter* logger);
    ~parser() = default;
    void parse(std::string const& fname);
    expr_ast const* parse_tree() const;
  private:
    utils::LogReporter* m_log;
    lexer::scanner* m_scanner;
    std::unique_ptr<expr_ast> m_tree;
    std::vector<lexer::token<std::string>>::iterator m_it;
    //
    // TODO: Need to design symbol table needed to keep track of
    // TODO: symbols found during parsing process (useful to generate
    // TODO: code).
    //
  private:
    lexer::token<std::string> getNextToken();
    expr_ast * parse_token_stream(std::string const& fname);
    expr_ast * parse_module(std::string const& fname);
    expr_ast * parse_procedure(std::string const& fname);
    void parse_end_expression(const std::string &fname, lexer::TokenType type);
    expr_ast * parse_const_declaration(const std::string &fname);
    expr_ast * parse_string(const std::string &fname);
    expr_ast * parse_binary_expression(const std::string &fname);

  };
}


#endif //OBERONC_PARSER_H

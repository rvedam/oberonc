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
    void parse_token_stream(std::string const& fname);
    void parse_module(std::string const& fname);
    void parse_procedure(std::string const& fname);
  private:
    utils::LogReporter* m_log;
    lexer::scanner* m_scanner;
    std::unique_ptr<expr_ast> m_tree;
    //
    // TODO: Need to design symbol table needed to keep track of
    // TODO: symbols found during parsing process (useful to generate
    // TODO: code).
    //
  };
}


#endif //OBERONC_PARSER_H

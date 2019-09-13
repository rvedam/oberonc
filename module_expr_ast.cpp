//
// Created by Ramnarayan Vedam on 2019-08-30.
//

#include "module_expr_ast.h"

namespace oberon
{
  module_expr_ast::module_expr_ast(std::string module_name) : m_name(module_name)
  {}

  std::string module_expr_ast::name() const
  {
    return m_name;
  }

  std::vector<expr_ast*> module_expr_ast::members() const
  { return m_members; }

  void module_expr_ast::add(oberon::expr_ast* node)
  {
    m_members.emplace_back(std::move(node));
  }
}
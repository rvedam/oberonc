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

  std::unordered_map<std::string, expr_ast*> module_expr_ast::module_members() const
  { return m_members; }

  void module_expr_ast::add_member(std::string name, oberon::expr_ast *node)
  {
    m_members[name] = node;
  }
}
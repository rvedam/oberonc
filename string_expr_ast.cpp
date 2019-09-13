//
// Created by Ramnarayan Vedam on 2019-09-08.
//

#include "string_expr_ast.h"

namespace oberon
{
  string_expr_ast::string_expr_ast(std::string value) : m_value(value)
  {
  }

  std::string string_expr_ast::value() const
  {
    return m_value;
  }
}
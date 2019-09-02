//
// Created by Ramnarayan Vedam on 2019-08-31.
//

#include "comment_expr_ast.h"

namespace oberon
{
  comment_expr_ast::comment_expr_ast(std::string comment) : m_comment(comment)
  {

  }

  std::string comment_expr_ast::comment() const
  {
    return m_comment;
  }
}
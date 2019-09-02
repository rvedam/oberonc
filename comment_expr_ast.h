//
// Created by Ramnarayan Vedam on 2019-08-31.
//

#ifndef OBERONC_COMMENT_EXPR_AST_H
#define OBERONC_COMMENT_EXPR_AST_H

#include <string>
#include "expr_ast.h"

namespace oberon
{
  class comment_expr_ast : public expr_ast
  {
  public:
    comment_expr_ast(std::string comment);
    std::string comment() const;
  private:
    std::string m_comment;
  };
}


#endif //OBERONC_COMMENT_EXPR_AST_H

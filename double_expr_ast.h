//
// Created by Ramnarayan Vedam on 2019-08-25.
//

#ifndef OBERONC_DOUBLE_EXPR_AST_H
#define OBERONC_DOUBLE_EXPR_AST_H

#include "expr_ast.h"

namespace oberon
{
  class double_expr_ast : public expr_ast
  {
  public:
    double_expr_ast(const double val);
  private:
    double m_double;
  };
}

#endif //OBERONC_DOUBLE_EXPR_AST_H

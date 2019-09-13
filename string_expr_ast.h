//
// Created by Ramnarayan Vedam on 2019-09-08.
//

#ifndef OBERONC_STRING_EXPR_AST_H
#define OBERONC_STRING_EXPR_AST_H

#include <string>
#include "expr_ast.h"

namespace oberon
{
  class string_expr_ast : public expr_ast
  {
  public:
    string_expr_ast(std::string value);
    ~string_expr_ast() = default;
    std::string value() const;

  private:
    std::string m_value;
  };
}


#endif //OBERONC_STRING_EXPR_AST_H

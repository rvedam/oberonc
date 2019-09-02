//
// Created by Ramnarayan Vedam on 2019-08-30.
//

#ifndef OBERONC_MODULE_EXPR_AST_H
#define OBERONC_MODULE_EXPR_AST_H

#include <string>
#include <unordered_map>
#include "expr_ast.h"

namespace oberon
{
  /**
   * @brief represents Oberon Modules (please refer to the standard for more info)
   */
  class module_expr_ast : public expr_ast
  {
  public:
    /**
     * constructs new module AST node
     * @param module_name name of Oberon Module getting compiled.
     */
    module_expr_ast(std::string module_name);

    /**
     * returns name of module expression.
     * @return name of module represented by AST Node.
     */
    std::string name() const;

    /**
     * Members of the module
     * @return unordered_map representing module members
     */
    std::unordered_map<std::string, expr_ast*> module_members() const;

    /**
     * add new member into module
     * @param name name of the specific variable
     * @param node AST node representing the expression in code.
     */
    void add_member(std::string name, expr_ast* node);

  private:
    std::string m_name;
    std::unordered_map<std::string, expr_ast*> m_members;
  };
}


#endif //OBERONC_MODULE_EXPR_AST_H

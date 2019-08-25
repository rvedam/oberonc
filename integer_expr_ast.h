//
// Created by Ramnarayan Vedam on 2019-08-18.
//

#pragma once

#include "expr_ast.h"

namespace oberon
{
    class integer_expr_ast : public expr_ast
    {
    public:
        integer_expr_ast(int val);
    private:
        int m_value;
    };
}
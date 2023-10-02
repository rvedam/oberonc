//
// Created by vedam on 10/20/22.
//

#pragma once

#include "Symbol.hpp"

/**
 * @brief represents Oberon array types
 */
class ArrayTypeSymbol : public Symbol
{
private:
  Symbol m_arrayType;
  int m_arraySize;    /// -1 if dynamic array otherwise sizeof(array)
  bool m_dynamic;     /// true if dynamic array
public:
  explicit ArrayTypeSymbol(std::string name, std::string module, Symbol arrayType, int arraySize,
                           bool isDynamic, bool exported);

  bool isDynamic() const;
  int size() const;
};

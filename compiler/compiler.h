#ifndef MOCKER_COMPILER_H
#define MOCKER_COMPILER_H

#include <iostream>
#include <string>

#include "parse/lexer.h"

namespace mocker {

class Compiler {
public:
  explicit Compiler(std::istream &input);

  void compile();

private:
  const std::string sourceCode;
  Lexer lex;

}; // class Compiler

} // namespace mocker

#endif // MOCKER_COMPILER_H

#ifndef MOCKER_COMPILER_H
#define MOCKER_COMPILER_H

#include <iostream>
#include <string>

namespace mocker {

class Compiler {
public:
  explicit Compiler(std::istream & input);

private:
  const std::string sourceCode;

}; // class Compiler

} // namespace mocker

#endif //MOCKER_COMPILER_H

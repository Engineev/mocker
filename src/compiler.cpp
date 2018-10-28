#include "compiler.h"

#include <sstream>

namespace mocker {

std::string readFromStream(std::istream & input) {
  std::stringstream ss;
  ss << input.rdbuf();
  return ss.str();
}

Compiler::Compiler(std::istream &input) : sourceCode(readFromStream(input)) {}


} // namespace mocker
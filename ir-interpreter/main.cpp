#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "interpreter.h"

//#define PRINT_EXITCODE

int main(int argc, char **argv) {
  std::string path = argv[1];
  std::ifstream fin(path);
  std::stringstream sstr;
  sstr << fin.rdbuf();
  std::string src = sstr.str();

  mocker::ir::Interpreter interpreter(src);
  auto exitcode = interpreter.run();

#ifdef PRINT_EXITCODE
  std::cout << "=============================\nexit: " << exitcode << std::endl;
#else
  return (int)exitcode;
#endif
}
#ifndef MOCKER_PRINTER_H
#define MOCKER_PRINTER_H

#include <iostream>
#include <string>

#include "module.h"

namespace mocker {
namespace ir {

std::string fmtAddr(const std::shared_ptr<Addr> &addr);

std::string fmtInst(const std::shared_ptr<IRInst> &inst);

std::string fmtGlobalVarDef(const GlobalVarModule &var);

class Printer {
public:
  explicit Printer(const Module &module, std::ostream &out = std::cout)
      : module(module), out(out) {}

  void operator()(bool printExternal = true) const;

private:
  void printFunc(const std::string &name, const FunctionModule &func) const;

  const Module &module;
  std::ostream &out;
};

} // namespace ir
} // namespace mocker

#endif // MOCKER_PRINTER_H

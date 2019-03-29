#ifndef MOCKER_PRINTER_H
#define MOCKER_PRINTER_H

#include <iostream>
#include <string>

#include "module.h"

namespace mocker {
namespace ir {

std::string fmtAddr(const std::shared_ptr<Addr> &addr);

std::string fmtInst(const std::shared_ptr<IRInst> &inst);

void printFunc(const FunctionModule &func, std::ostream &out = std::cout);

void printModule(const Module &module, std::ostream &out = std::cout);

} // namespace ir
} // namespace mocker

#endif // MOCKER_PRINTER_H

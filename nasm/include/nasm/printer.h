#ifndef MOCKER_NASM_PRINTER_H
#define MOCKER_NASM_PRINTER_H

#include <iostream>

#include "module.h"

namespace mocker {
namespace nasm {

std::string fmtDirective(const std::shared_ptr<Directive> &directive);

std::string fmtAddr(const std::shared_ptr<Addr> &addr);

std::string fmtInst(const std::shared_ptr<Inst> &inst);

std::string fmtLine(const Line &line);

void printSection(const Section &section, std::ostream &out = std::cout);

void printModule(const Module &module, std::ostream &out = std::cout);

} // namespace nasm
} // namespace mocker

#endif // MOCKER_NASM_PRINTER_H

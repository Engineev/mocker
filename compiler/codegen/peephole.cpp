#include "peephole.h"

#include "nasm/addr.h"

namespace mocker {
namespace {

void useIncDec(nasm::Module &module) {
  for (auto &line : module.getSection(".text").getLines()) {
    auto &inst = line.inst;
    if (!inst)
      continue;
    auto incDec = nasm::dyc<nasm::BinaryInst>(inst);
    if (!incDec)
      continue;
    auto op = incDec->getType();
    if (op != nasm::BinaryInst::Add && op != nasm::BinaryInst::Sub)
      continue;
    auto lit = nasm::dyc<nasm::NumericConstant>(incDec->getRhs());
    if (!lit || lit->getVal() != 1)
      continue;
    inst = std::make_shared<nasm::UnaryInst>(
        op == nasm::BinaryInst::Add ? nasm::UnaryInst::Inc
                                    : nasm::UnaryInst::Dec,
        nasm::dyc<nasm::Register>(incDec->getLhs()));
  }
}

} // namespace

nasm::Module runPeepholeOptimization(const nasm::Module &module) {
  auto res = module;
  useIncDec(res);
  return res;
}

} // namespace mocker

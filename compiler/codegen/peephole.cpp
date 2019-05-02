#include "peephole.h"

#include "nasm/addr.h"
#include "nasm/helper.h"

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

// I1: a += b   ; <- This can be removed
// I2: a = c
void removeUnusedValue(nasm::Module &module) {
  auto &lines = module.getSection(".text").getLines();

  for (auto iter = lines.begin(), end = lines.end(); iter != end; ++iter) {
    auto &inst = iter->inst;
    if (!inst)
      continue;
    const auto mov = nasm::dyc<nasm::Mov>(inst);
    if (!mov)
      continue;
    auto dest = nasm::dyc<nasm::Register>(mov->getDest());
    if (!dest)
      continue;
    auto used = nasm::getUsedRegs(mov);
    bool flag = true;
    for (auto &reg : used) {
      if (reg->getIdentifier() == dest->getIdentifier()) {
        flag = false;
        break;
      }
    }
    if (!flag)
      continue;

    for (auto riter = std::make_reverse_iterator(iter), rend = lines.rend();
         riter != rend; ++riter) {
      auto &rInst = riter->inst;
      if (!rInst)
        break;
      std::shared_ptr<nasm::Register> rDest = nullptr;
      if (auto p = nasm::dyc<nasm::Mov>(rInst)) {
        rDest = nasm::dyc<nasm::Register>(p->getDest());
      }
      if (auto p = nasm::dyc<nasm::UnaryInst>(rInst)) {
        rDest = p->getReg();
      }
      if (auto p = nasm::dyc<nasm::BinaryInst>(rInst)) {
        rDest = nasm::dyc<nasm::Register>(p->getLhs());
      }
      if (!rDest || rDest->getIdentifier() != dest->getIdentifier())
        break;
      rInst = std::make_shared<nasm::Empty>();
    }
  }

  lines.remove_if([](const nasm::Line &line) {
    return line.label.empty() && nasm::dyc<nasm::Empty>(line.inst);
  });
}

} // namespace

nasm::Module runPeepholeOptimization(const nasm::Module &module) {
  auto res = module;
  useIncDec(res);
  removeUnusedValue(res);
  return res;
}

} // namespace mocker

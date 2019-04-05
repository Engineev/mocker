#include "helper.h"

#include <cassert>
#include <vector>

namespace mocker {
namespace nasm {
namespace {

std::vector<std::shared_ptr<Register>>
merge(const std::vector<std::shared_ptr<Register>> &lhs,
      const std::vector<std::shared_ptr<Register>> &rhs) {
  auto res = lhs;
  for (auto &item : rhs)
    res.emplace_back(item);
  return res;
}

} // namespace

std::vector<std::shared_ptr<Register>>
getInvolvedRegs(const std::shared_ptr<Addr> &addr) {
  if (dyc<NumericConstant>(addr) || dyc<Label>(addr) || dyc<LabelAddr>(addr))
    return {};
  if (auto p = dyc<Register>(addr))
    return {p};
  if (auto p = dyc<MemoryAddr>(addr)) {
    std::vector<std::shared_ptr<Register>> res;
    if (p->getReg1())
      res.emplace_back(p->getReg1());
    if (p->getReg2())
      res.emplace_back(p->getReg2());
    return res;
  }
  assert(false);
}

std::vector<std::shared_ptr<Register>>
getInvolvedRegs(const std::shared_ptr<Inst> &inst) {
  if (dyc<Empty>(inst) || dyc<Jmp>(inst) || dyc<CJump>(inst) ||
      dyc<Leave>(inst) || dyc<Ret>(inst) || dyc<AlignStack>(inst) ||
      dyc<Call>(inst) || dyc<Cqto>(inst))
    return {};
  if (auto p = dyc<Mov>(inst)) {
    return merge(getInvolvedRegs(p->getDest()),
                 getInvolvedRegs(p->getOperand()));
  }
  if (auto p = dyc<BinaryInst>(inst)) {
    return merge(getInvolvedRegs(p->getLhs()), getInvolvedRegs(p->getRhs()));
  }
  if (auto p = dyc<Push>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<Pop>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<Lea>(inst)) {
    return merge({p->getDest()}, getInvolvedRegs(p->getAddr()));
  }
  if (auto p = dyc<Cmp>(inst)) {
    return merge(getInvolvedRegs(p->getLhs()), getInvolvedRegs(p->getRhs()));
  }
  if (auto p = dyc<Set>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<UnaryInst>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<IDivq>(inst)) {
    return {getInvolvedRegs(p->getRhs())};
  }
  assert(false);
}

} // namespace nasm
} // namespace mocker
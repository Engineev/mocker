#include "helper.h"

#include <cassert>
#include <unordered_set>
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
  auto used = getUsedRegs(inst);
  auto defined = getDefinedRegs(inst);
  auto ud = merge(used, defined);
  std::unordered_set<std::shared_ptr<Register>, RegPtrHash, RegPtrEqual> unique(
      ud.begin(), ud.end());
  return std::vector<std::shared_ptr<Register>>(unique.begin(), unique.end());
}

std::vector<std::shared_ptr<Register>>
getUsedRegs(const std::shared_ptr<Inst> &inst) {
  if (dyc<Empty>(inst) || dyc<Pop>(inst) || dyc<Jmp>(inst) || dyc<Cqo>(inst) ||
      dyc<CJump>(inst) || dyc<Set>(inst))
    return {};

  if (auto p = dyc<Call>(inst)) {
    return {rdi(), rsi(), rdx(), rcx(), r8(), r9()};
  }

  if (auto p = dyc<Mov>(inst)) {
    auto res = getInvolvedRegs(p->getOperand());
    if (!dyc<Register>(p->getDest()))
      res = merge(res, getInvolvedRegs(p->getDest()));
    return res;
  }

  if (auto p = dyc<Lea>(inst)) {
    return getInvolvedRegs(p->getAddr());
  }
  if (auto p = dyc<UnaryInst>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<BinaryInst>(inst)) {
    return merge(getInvolvedRegs(p->getLhs()), getInvolvedRegs(p->getRhs()));
  }
  if (auto p = dyc<Push>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<Leave>(inst)) {
    return {rbp()};
  }
  if (auto p = dyc<Cmp>(inst)) {
    return merge(getInvolvedRegs(p->getLhs()), getInvolvedRegs(p->getRhs()));
  }
  if (auto p = dyc<IDiv>(inst)) {
    return merge({rax(), rdx()}, getInvolvedRegs(p->getRhs()));
  }
  if (auto p = dyc<Ret>(inst)) {
    return {rbp(), rbx(), r12(), r13(), r14(), r15()};
  }
  assert(false);
}

std::vector<std::shared_ptr<Register>>
getDefinedRegs(const std::shared_ptr<Inst> &inst) {
  if (dyc<Empty>(inst) || dyc<Ret>(inst) || dyc<Push>(inst) || dyc<Jmp>(inst) ||
      dyc<CJump>(inst) || dyc<Cmp>(inst)) {
    return {};
  }
  if (auto p = dyc<Mov>(inst)) {
    if (auto reg = dyc<nasm::Register>(p->getDest()))
      return {reg};
    return {};
  }
  if (auto p = dyc<Lea>(inst)) {
    return {p->getDest()};
  }
  if (auto p = dyc<UnaryInst>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<BinaryInst>(inst)) {
    return {dyc<Register>(p->getLhs())};
  }
  if (auto p = dyc<Call>(inst)) {
    return {rax(), rcx(), rdx(), rsp(), rsi(), rdi(), r8(), r9(), r10(), r11()};
  }
  if (auto p = dyc<Pop>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<Leave>(inst)) {
    return {rsp(), rbp()};
  }
  if (auto p = dyc<Set>(inst)) {
    return {p->getReg()};
  }
  if (auto p = dyc<IDiv>(inst)) {
    return {rax(), rdx()};
  }
  assert(false);
}

std::vector<std::shared_ptr<EffectiveAddr>>
getInvolvedMem(const std::shared_ptr<Inst> &inst) {
  std::vector<std::shared_ptr<EffectiveAddr>> res;

  if (auto p = dyc<Mov>(inst)) {
    if (auto m = dyc<EffectiveAddr>(p->getOperand()))
      res.emplace_back(m);
    if (auto m = dyc<EffectiveAddr>(p->getDest()))
      res.emplace_back(m);
    return res;
  }

  if (auto p = dyc<Lea>(inst)) {
    return {dyc<EffectiveAddr>(p->getAddr())};
  }

  if (auto p = dyc<BinaryInst>(inst)) {
    if (auto m = dyc<EffectiveAddr>(p->getRhs()))
      res.emplace_back(m);
    return res;
  }

  return {};
}

std::shared_ptr<Addr> replaceRegs(const std::shared_ptr<Addr> &addr,
                                  const RegMap<std::shared_ptr<Register>> &mp) {
  auto getIfExists = [&mp](const std::shared_ptr<Register> &p) {
    auto iter = mp.find(p);
    if (iter == mp.end())
      return p;
    return iter->second;
  };
  if (dyc<NumericConstant>(addr) || dyc<LabelAddr>(addr) || dyc<Label>(addr))
    return addr;
  if (auto p = dyc<Register>(addr))
    return getIfExists(p);
  if (auto p = dyc<MemoryAddr>(addr)) {
    auto reg1 = dyc<Register>(p->getReg1());
    reg1 = reg1 ? getIfExists(reg1) : p->getReg1();
    auto reg2 = dyc<Register>(p->getReg2());
    reg2 = reg2 ? getIfExists(reg2) : p->getReg2();
    return std::make_shared<MemoryAddr>(reg1, reg2, p->getScale(),
                                        p->getNumber());
  }
  assert(false);
}

std::shared_ptr<Inst> replaceRegs(const std::shared_ptr<Inst> &inst,
                                  const RegMap<std::shared_ptr<Register>> &mp) {
  if (dyc<Empty>(inst) || dyc<Ret>(inst) || dyc<Call>(inst) ||
      dyc<Leave>(inst) || dyc<Jmp>(inst) || dyc<CJump>(inst) || dyc<Cqo>(inst))
    return inst;
  if (auto p = dyc<Mov>(inst)) {
    return std::make_shared<Mov>(replaceRegs(p->getDest(), mp),
                                 replaceRegs(p->getOperand(), mp));
  }
  if (auto p = dyc<Lea>(inst)) {
    return std::make_shared<Lea>(dyc<Register>(replaceRegs(p->getDest(), mp)),
                                 replaceRegs(p->getAddr(), mp));
  }
  if (auto p = dyc<UnaryInst>(inst)) {
    return std::make_shared<UnaryInst>(
        p->getOp(), dyc<Register>(replaceRegs(p->getReg(), mp)));
  }
  if (auto p = dyc<BinaryInst>(inst)) {
    return std::make_shared<BinaryInst>(p->getType(),
                                        replaceRegs(p->getLhs(), mp),
                                        replaceRegs(p->getRhs(), mp));
  }
  if (auto p = dyc<Push>(inst)) {
    return std::make_shared<Push>(dyc<Register>(replaceRegs(p->getReg(), mp)));
  }
  if (auto p = dyc<Pop>(inst)) {
    return std::make_shared<Pop>(dyc<Register>(replaceRegs(p->getReg(), mp)));
  }
  if (auto p = dyc<Cmp>(inst)) {
    return std::make_shared<Cmp>(replaceRegs(p->getLhs(), mp),
                                 replaceRegs(p->getRhs(), mp));
  }
  if (auto p = dyc<Set>(inst)) {
    return std::make_shared<Set>(p->getOp(),
                                 dyc<Register>(replaceRegs(p->getReg(), mp)));
  }
  if (auto p = dyc<IDiv>(inst)) {
    return std::make_shared<IDiv>(replaceRegs(p->getRhs(), mp));
  }
  assert(false);
}

} // namespace nasm
} // namespace mocker
#include "naive_register_allocation.h"

#include <cassert>
#include <unordered_map>
#include <unordered_set>

#include "helper.h"
#include "nasm/addr.h"
#include "nasm/helper.h"

namespace mocker {
namespace {

using nasm::RegMap;
using nasm::RegSet;

std::shared_ptr<nasm::Register>
dycVReg(const std::shared_ptr<nasm::Addr> &addr) {
  auto reg = nasm::dyc<nasm::Register>(addr);
  if (!reg || reg->getIdentifier().at(0) != 'v')
    return nullptr;
  return reg;
}

std::shared_ptr<nasm::Register>
dycPReg(const std::shared_ptr<nasm::Addr> &addr) {
  auto reg = nasm::dyc<nasm::Register>(addr);
  if (!reg || reg->getIdentifier().at(0) == 'v')
    return nullptr;
  return reg;
}

RegMap<std::size_t> buildVReg2Offsets(LineIter funcBeg, LineIter funcEnd) {
  std::size_t vRegCnt = 0;
  RegMap<std::size_t> offsets;

  for (auto iter = funcBeg; iter != funcEnd; ++iter) {
    if (!iter->inst)
      continue;
    auto regs = nasm::getInvolvedRegs(iter->inst);
    for (auto &reg : regs) {
      if (reg->getIdentifier().at(0) != 'v')
        continue;
      if (offsets.find(reg) != offsets.end())
        continue;
      ++vRegCnt;
      offsets[reg] = vRegCnt * 8;
    }
  }

  return offsets;
}

} // namespace

namespace {

void loadVReg(const std::shared_ptr<nasm::Register> &dest,
              const std::shared_ptr<nasm::Register> &src,
              const RegMap<std::size_t> &offsets, nasm::Section &text) {
  auto offset = offsets.at(src);
  auto memAddr = std::make_shared<nasm::MemoryAddr>(nasm::rbp(), -offset);
  text.emplaceInst<nasm::Mov>(dest, memAddr);
}

void storeVReg(const std::shared_ptr<nasm::Register> &dest,
               const std::shared_ptr<nasm::Addr> &val,
               const RegMap<std::size_t> &offsets, nasm::Section &text) {
  auto offset = offsets.at(dest);
  auto memAddr = std::make_shared<nasm::MemoryAddr>(nasm::rbp(), -offset);
  text.emplaceInst<nasm::Mov>(memAddr, val);
}

void allocateMov(const std::shared_ptr<nasm::Mov> &p,
                 const RegMap<std::size_t> &offsets, nasm::Section &text) {
  // For mov D, S, we first move the value of S into r10 and, then, move r10
  // into D

  // move S into rax
  auto operand = p->getOperand();
  if (auto c = nasm::dyc<nasm::NumericConstant>(operand)) {
    text.emplaceInst<nasm::Mov>(nasm::r10(), c);
  } else if (auto pReg = dycPReg(operand)) { // physical register
    text.emplaceInst<nasm::Mov>(nasm::r10(), pReg);
  } else if (auto vReg = dycVReg(operand)) {
    loadVReg(nasm::r10(), vReg, offsets, text);
  } else if (auto labelAddr = nasm::dyc<nasm::LabelAddr>(operand)) {
    text.emplaceInst<nasm::Mov>(nasm::r10(), labelAddr);
  } else if (auto memAddr = nasm::dyc<nasm::MemoryAddr>(operand)) {
    assert(!memAddr->getReg2());
    auto reg1 = memAddr->getReg1();
    if (dycVReg(reg1)) {
      loadVReg(nasm::r11(), reg1, offsets, text);
      reg1 = nasm::r11();
    }
    text.emplaceInst<nasm::Mov>(nasm::r10(), std::make_shared<nasm::MemoryAddr>(
                                                 reg1, memAddr->getNumber()));
  } else {
    assert(false);
  }

  // move r10 into D
  auto dest = p->getDest();
  if (auto pReg = dycPReg(dest)) {
    text.emplaceInst<nasm::Mov>(pReg, nasm::r10());
    return;
  }
  if (auto vReg = dycVReg(dest)) {
    storeVReg(vReg, nasm::r10(), offsets, text);
    return;
  }
  if (auto labelAddr = nasm::dyc<nasm::LabelAddr>(dest)) {
    text.emplaceInst<nasm::Mov>(labelAddr, nasm::r10());
    return;
  }
  if (auto memAddr = nasm::dyc<nasm::MemoryAddr>(dest)) {
    assert(!memAddr->getReg2());
    auto reg1 = memAddr->getReg1();
    if (dycVReg(reg1)) {
      loadVReg(nasm::r11(), reg1, offsets, text);
      reg1 = nasm::r11();
    }
    text.emplaceInst<nasm::Mov>(
        std::make_shared<nasm::MemoryAddr>(nasm::r11(), memAddr->getNumber()),
        nasm::r10());
    return;
  }

  assert(false);
}

void allocate(const std::shared_ptr<nasm::Inst> &inst,
              const RegMap<std::size_t> &offsets, nasm::Section &text) {
  if (nasm::getInvolvedRegs(inst).empty() || nasm::dyc<nasm::Call>(inst) ||
      nasm::dyc<nasm::Leave>(inst) || nasm::dyc<nasm::Ret>(inst)) {
    text.appendInst(inst);
    return;
  }
  auto load = [&offsets, &text](const std::shared_ptr<nasm::Register> &dest,
                                const std::shared_ptr<nasm::Register> &src) {
    loadVReg(dest, src, offsets, text);
  };
  auto store = [&offsets, &text](const std::shared_ptr<nasm::Register> &dest,
                                 const std::shared_ptr<nasm::Addr> &val) {
    storeVReg(dest, val, offsets, text);
  };

  if (auto p = nasm::dyc<nasm::Mov>(inst)) {
    allocateMov(p, offsets, text);
    return;
  }
  if (auto p = nasm::dyc<nasm::BinaryInst>(inst)) {
    auto rhs = p->getRhs();
    if (auto reg = dycVReg(rhs)) {
      load(nasm::r10(), reg);
      rhs = nasm::r10();
    }
    auto lhs = p->getLhs();
    if (auto reg = dycVReg(lhs)) {
      load(nasm::r11(), reg);
      lhs = nasm::r11();
    }
    text.emplaceInst<nasm::BinaryInst>(p->getType(), lhs, rhs);
    if (auto reg = dycVReg(p->getLhs()))
      store(reg, lhs);
    return;
  }
  if (auto p = nasm::dyc<nasm::Push>(inst)) {
    auto reg = dycVReg(p->getReg());
    assert(reg);
    load(nasm::r10(), reg);
    text.emplaceInst<nasm::Push>(nasm::r10());
    return;
  }
  if (auto p = nasm::dyc<nasm::Pop>(inst)) {
    auto reg = dycVReg(p->getReg());
    assert(reg);
    text.emplaceInst<nasm::Pop>(nasm::r10());
    store(reg, nasm::r10());
    return;
  }
  if (auto p = nasm::dyc<nasm::Lea>(inst)) {
    auto reg = dycVReg(p->getDest());
    if (!reg)
      return;
    text.emplaceInst<nasm::Lea>(nasm::r10(), p->getAddr());
    store(reg, nasm::r10());
    return;
  }
  if (auto p = nasm::dyc<nasm::Cmp>(inst)) {
    auto lhs = p->getLhs();
    if (auto reg = dycVReg(lhs)) {
      loadVReg(nasm::r10(), reg, offsets, text);
      lhs = nasm::r10();
    }
    auto rhs = p->getRhs();
    if (auto reg = dycVReg(rhs)) {
      loadVReg(nasm::r11(), reg, offsets, text);
      rhs = nasm::r11();
    }
    text.emplaceInst<nasm::Cmp>(lhs, rhs);
    return;
  }
  if (auto p = nasm::dyc<nasm::Set>(inst)) {
    auto reg = dycVReg(p->getReg());
    if (!reg) {
      text.appendInst(p);
      return;
    }
    auto tmp = std::make_shared<nasm::Register>("r10b");
    text.emplaceInst<nasm::Set>(p->getOp(), tmp);
    storeVReg(reg, nasm::r10(), offsets, text);
    return;
  }
  if (auto p = nasm::dyc<nasm::UnaryInst>(inst)) {
    auto reg = dycVReg(p->getReg());
    if (!reg) {
      text.appendInst(p);
      return;
    }
    load(nasm::r10(), reg);
    text.emplaceInst<nasm::UnaryInst>(p->getOp(), nasm::r10());
    store(reg, nasm::r10());
    return;
  }
  if (auto p = nasm::dyc<nasm::IDiv>(inst)) {
    auto reg = dycVReg(p->getRhs());
    if (!reg) {
      text.appendInst(p);
      return;
    }
    load(nasm::r10(), reg);
    text.emplaceInst<nasm::IDiv>(nasm::r10());
    return;
  }

  assert(false);
}

void allocate(LineIter funcBeg, LineIter funcEnd, nasm::Section &text) {
  RegMap<std::size_t> offsets = buildVReg2Offsets(funcBeg, funcEnd);
  std::size_t vRegCnt = offsets.size();

  auto lineIter = funcBeg;
  text.appendLine(*lineIter);
  ++lineIter;
  assert(nasm::dyc<nasm::Push>(lineIter->inst));
  text.appendLine(*lineIter);
  ++lineIter;
  assert(nasm::dyc<nasm::Mov>(lineIter->inst));
  text.appendLine(*lineIter);
  ++lineIter;
  // align
  std::size_t alignSz = 8 * vRegCnt + (16 - (8 * vRegCnt) % 16);
  if (alignSz)
    text.emplaceInst<nasm::BinaryInst>(
        nasm::BinaryInst::Sub, nasm::rsp(),
        std::make_shared<nasm::NumericConstant>(alignSz));

  for (; lineIter != funcEnd; ++lineIter) {
    if (!lineIter->inst) {
      text.appendLine(*lineIter);
      continue;
    }
    allocate(lineIter->inst, offsets, text);
  }
}

nasm::Section allocate(const nasm::Section &section) {
  nasm::Section res(section.getName());

  std::vector<LineIter> funcBegs;
  for (auto iter = section.getLines().begin(); iter != section.getLines().end();
       ++iter) {
    if (!iter->inst && iter->label.at(0) != '.') {
      funcBegs.emplace_back(iter);
    }
  }

  funcBegs.emplace_back(section.getLines().end());
  for (auto iter = funcBegs.begin(), ed = funcBegs.end() - 1; iter != ed;
       ++iter) {
    allocate(*iter, *(iter + 1), res);
  }

  return res;
}

} // namespace
} // namespace mocker

namespace mocker {

nasm::Module allocateRegistersNaively(const nasm::Module &module) {
  nasm::Module res(module);
  res.getSection(".text") = allocate(res.getSection(".text"));
  return res;
}

} // namespace mocker
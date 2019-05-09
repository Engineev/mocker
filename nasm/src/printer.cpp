#include "printer.h"

#include <cassert>
#include <iomanip>
#include <sstream>

#include "small_map.h"

namespace mocker {
namespace nasm {

std::string fmtDirective(const std::shared_ptr<Directive> &directive) {
  if (auto p = dyc<Default>(directive))
    return "default " + p->getMode();
  if (auto p = dyc<Global>(directive))
    return "global " + p->getIdentifier();
  if (auto p = dyc<Extern>(directive)) {
#ifdef ONLINE_JUDGE_SUPPORT
    return "";
#endif
    return "extern " + p->getIdentifier();
  }
  assert(false);
}

std::string fmtAddr(const std::shared_ptr<Addr> &addr) {
  if (auto p = dyc<NumericConstant>(addr))
    return std::to_string(p->getVal());
  if (auto p = dyc<Register>(addr))
    return p->getIdentifier();
  if (auto p = dyc<MemoryAddr>(addr)) {
    std::string res = "[";
    if (p->getReg1())
      res += fmtAddr(p->getReg1());
    if (p->getReg2()) {
      if (res.size() != 1)
        res += "+";
      res += fmtAddr(p->getReg2()) + "*" + std::to_string(p->getScale());
    }
    if (p->getNumber() != 0) {
      if (p->getNumber() > 0)
        res += "+";
      res += std::to_string(p->getNumber());
    }
    return res + "]";
  }
  if (auto p = dyc<Label>(addr)) {
    return p->getName();
  }
  if (auto p = dyc<LabelAddr>(addr)) {
    return "[" + p->getLabelName() + "]";
  }
  assert(false);
}

std::string fmtInst(const std::shared_ptr<Inst> &inst) {
  using namespace std::string_literals;

  if (auto p = dyc<Db>(inst)) {
    const auto &bytes = p->getData();
    std::string res = "db " + std::to_string(bytes[0]);
    for (std::size_t i = 1; i < bytes.size(); ++i) {
      res += ", " + std::to_string(bytes[i]);
    }
    return res;
  }
  if (auto p = dyc<Resb>(inst)) {
    return "resb " + std::to_string(p->getSize());
  }
  if (auto p = dyc<Mov>(inst)) {
    std::string res = "mov ";
    if (dyc<EffectiveAddr>(p->getDest()))
      res += "qword ";
    return res + fmtAddr(p->getDest()) + ", " + fmtAddr(p->getOperand());
  }
  if (auto p = dyc<Ret>(inst)) {
    return "ret";
  }
  if (auto p = dyc<BinaryInst>(inst)) {
    if (p->getType() == BinaryInst::Sal || p->getType() == BinaryInst::Sar) {
      auto rhs = p->getRhs();
      if (nasm::dyc<nasm::Register>(rhs)) {
        assert(nasm::dyc<nasm::Register>(p->getRhs())->getIdentifier() ==
               "rcx");
        return (p->getType() == BinaryInst::Sal ? "sal " : "sar ") +
               fmtAddr(p->getLhs()) + ", cl";
      }
      return (p->getType() == BinaryInst::Sal ? "sal " : "sar ") +
             fmtAddr(p->getLhs()) + ", " + fmtAddr(p->getRhs());
    }

    static SmallMap<BinaryInst::OpType, std::string> opName{
        {BinaryInst::BitOr, "or"s}, {BinaryInst::BitAnd, "and"s},
        {BinaryInst::Xor, "xor"s},  {BinaryInst::Add, "add"s},
        {BinaryInst::Sub, "sub"s},  {BinaryInst::Mul, "imul"s},
    };
    return opName.at(p->getType()) + " " + fmtAddr(p->getLhs()) + ", " +
           fmtAddr(p->getRhs());
  }
  if (auto p = dyc<Push>(inst)) {
    return "push " + fmtAddr(p->getReg());
  }
  if (auto p = dyc<Pop>(inst)) {
    return "pop " + fmtAddr(p->getReg());
  }
  if (dyc<Leave>(inst)) {
    return "leave";
  }
  if (auto p = dyc<Call>(inst)) {
    return "call " + p->getFuncName();
  }
  if (auto p = dyc<Lea>(inst)) {
    return "lea " + fmtAddr(p->getDest()) + ", " + fmtAddr(p->getAddr());
  }
  if (auto p = dyc<Jmp>(inst)) {
    return "jmp " + fmtAddr(p->getLabel());
  }
  if (auto p = dyc<Cmp>(inst)) {
    return "cmp " + fmtAddr(p->getLhs()) + ", " + fmtAddr(p->getRhs());
  }
  if (auto p = dyc<Set>(inst)) {
    static const SmallMap<Set::OpType, std::string> opName{
        {Set::OpType::Eq, "sete"}, {Set::OpType::Ne, "setne"},
        {Set::OpType::Lt, "setl"}, {Set::OpType::Le, "setle"},
        {Set::OpType::Gt, "setg"}, {Set::OpType::Ge, "setge"},
    };
    assert(p->getReg()->getIdentifier() == nasm::rax()->getIdentifier());
    return opName.at(p->getOp()) + " al";
  }
  if (auto p = dyc<CJump>(inst)) {
    static const SmallMap<CJump::OpType, std::string> opName{
        {CJump::OpType::Ez, "jz"}, {CJump::OpType::Ne, "jne"},
        {CJump::OpType::Lt, "jl"}, {CJump::OpType::Le, "jle"},
        {CJump::OpType::Gt, "jg"}, {CJump::OpType::Ge, "jge"},
    };
    return opName.at(p->getOp()) + " " + fmtAddr(p->getLabel());
  }
  if (auto p = dyc<UnaryInst>(inst)) {
    if (p->getOp() == UnaryInst::Neg)
      return "neg " + fmtAddr(p->getReg());
    if (p->getOp() == UnaryInst::Not)
      return "not " + fmtAddr(p->getReg());
    if (p->getOp() == UnaryInst::Inc)
      return "inc " + fmtAddr(p->getReg());
    if (p->getOp() == UnaryInst::Dec)
      return "dec " + fmtAddr(p->getReg());
    assert(false);
  }
  if (auto p = dyc<Cqo>(inst)) {
    return "cdq";
    return "cqo";
  }
  if (auto p = dyc<IDiv>(inst)) {
    static const nasm::RegMap<std::string> name {
        {rax(), "eax"},
        {rcx(), "ecx"},
        {rdx(), "edx"},
        {rbx(), "ebx"},
        {rsp(), "esp"},
        {rbp(), "ebp"},
        {rsi(), "esi"},
        {rdi(), "edi"},
        {r8(), "r8d"},
        {r9(), "r9d"},
        {r10(), "r10d"},
        {r11(), "r11d"},
        {r12(), "r12d"},
        {r13(), "r13d"},
        {r14(), "r14d"},
        {r15(), "r15d"},
    };
    return "idiv " + name.at(dyc<Register>(p->getRhs()));
    return "idiv " + fmtAddr(p->getRhs());
  }
  assert(false);
}

std::string fmtLine(const Line &line) {
  const std::size_t IndentSz = 8;
  auto instStr = (bool)line.inst ? fmtInst(line.inst) : "";
  if (line.label.empty())
    return std::string(IndentSz, ' ') + instStr;
  if (line.label.length() <= IndentSz - 2) {
    auto prefix = line.label + ": ";
    prefix.resize(IndentSz, ' ');
    return prefix + instStr;
  }
  if (!instStr.empty())
    return line.label + ":\n" + std::string(IndentSz, ' ') + instStr;
  return line.label + ":";
}

void printSection(const Section &section, std::ostream &out) {
  out << "SECTION " << section.getName() << std::endl;
  for (auto &line : section.getLines()) {
    if (line.label.empty() && !line.inst)
      continue;
    out << fmtLine(line) << std::endl;
  }
  std::flush(out);
}

void printModule(const Module &module, std::ostream &out) {
  for (auto &directive : module.getDirectives())
    out << fmtDirective(directive) << '\n';
  out << std::endl;

  for (auto &kv : module.getSections()) {
    printSection(kv.second, out);
    out << std::endl;
  }
}

} // namespace nasm
} // namespace mocker

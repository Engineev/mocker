#include "naive_instruction_selection.h"

#include <cassert>
#include <cstddef>
#include <unordered_map>

#include <iostream>

#include "ir/helper.h"
#include "nasm/addr.h"
#include "nasm/inst.h"
#include "optim/analysis/defuse.h"
#include "small_map.h"

namespace mocker {
namespace {

const std::vector<std::shared_ptr<nasm::Register>> CalleeSave = {
    nasm::rbp(), nasm::rbx(), nasm::r12(),
    nasm::r13(), nasm::r14(), nasm::r15()};

std::shared_ptr<nasm::LabelAddr>
makeLabelAddr(const std::shared_ptr<ir::Reg> &reg) {
  return std::make_shared<nasm::LabelAddr>("L" + reg->getIdentifier());
}

// replace "#" with "__"
std::string renameIdentifier(const std::string &ident) {
  std::string res;
  for (auto ch : ident) {
    if (ch == '#') {
      res += "__";
      continue;
    }
    res.push_back(ch);
  }
  return res;
}

class Context {
public:
  explicit Context(nasm::Section &text) {}

  void init(const ir::FunctionModule &func) { defUse.init(func); }

  std::shared_ptr<nasm::Register> newVirtualReg() {
    return std::make_shared<nasm::Register>("v" + std::to_string(vRegCnt++));
  }

  void addStackVar(const std::string &ident) {
    stackVar2Reg.emplace(ident, newVirtualReg());
  }

  bool isStackVar(const std::string &ident) const {
    return stackVar2Reg.find(ident) != stackVar2Reg.end();
  }

  const std::shared_ptr<nasm::Register> &
  getStackVarReg(const std::string &ident) {
    return stackVar2Reg.at(ident);
  }

  std::shared_ptr<nasm::Addr> getAddr(const std::shared_ptr<ir::Addr> &addr) {
    if (auto p = ir::dyc<ir::IntLiteral>(addr)) {
      return std::make_shared<nasm::NumericConstant>(p->getVal());
    }
    if (auto p = ir::dycLocalReg(addr)) {
      auto iter = irRegAddr.find(p->getIdentifier());
      if (iter != irRegAddr.end())
        return iter->second;
      return newVirtualRegFor(p);
    }
    if (auto p = ir::dycGlobalReg(addr)) {
      return irRegAddr.at(p->getIdentifier());
    }
    assert(false);
  }

  void setIrRegAddr(const std::shared_ptr<ir::Reg> &irReg,
                    const std::shared_ptr<nasm::Register> &vReg) {
    irRegAddr[irReg->getIdentifier()] = vReg;
  }

  const std::shared_ptr<nasm::Register> &
  getIrRegAddr(const std::shared_ptr<ir::Reg> &irReg) const {
    return irRegAddr.at(irReg->getIdentifier());
  }

  void setPRegAddr(const std::shared_ptr<nasm::Register> &pReg,
                   const std::shared_ptr<nasm::Register> &vReg) {
    pRegAddr[pReg] = vReg;
  }

  const std::shared_ptr<nasm::Register> &
  getPRegAddr(const std::shared_ptr<nasm::Register> &reg) const {
    return pRegAddr.at(reg);
  }

  const std::vector<typename DefUseChain::Use>
  getUses(const std::shared_ptr<ir::Reg> &reg) const {
    return defUse.getUses(reg);
  }

private:
  std::shared_ptr<nasm::Register>
  newVirtualRegFor(const std::shared_ptr<ir::Reg> &reg) {
    auto res = newVirtualReg();
    irRegAddr[reg->getIdentifier()] = res;
    return res;
  }

private:
  std::size_t vRegCnt = 0;
  std::unordered_map<std::string, std::shared_ptr<nasm::Register>> stackVar2Reg;

  nasm::RegMap<std::shared_ptr<nasm::Register>> pRegAddr;
  std::unordered_map<std::string, std::shared_ptr<nasm::Register>> irRegAddr;

  DefUseChain defUse;
};

void genStore(nasm::Section &text, Context &ctx,
              const std::shared_ptr<ir::Store> &p) {
  auto val = ctx.getAddr(p->getVal());
  auto irAddr = ir::dyc<ir::Reg>(p->getAddr());
  assert(irAddr);
  if (ctx.isStackVar(irAddr->getIdentifier())) {
    auto addr = ctx.getStackVarReg(irAddr->getIdentifier());
    text.emplaceInst<nasm::Mov>(addr, val);
    return;
  }
  if (auto global = ir::dycGlobalReg(irAddr)) {
    text.emplaceInst<nasm::Mov>(makeLabelAddr(global), val);
    return;
  }

  text.emplaceInst<nasm::Mov>(
      std::make_shared<nasm::MemoryAddr>(ctx.getAddr(irAddr)), val);
}

void genLoad(nasm::Section &text, Context &ctx,
             const std::shared_ptr<ir::Load> &p) {
  auto dest = ctx.getAddr(p->getDest());
  auto irAddr = ir::dyc<ir::Reg>(p->getAddr());
  assert(irAddr);
  if (ctx.isStackVar(irAddr->getIdentifier())) {
    auto val = ctx.getStackVarReg(irAddr->getIdentifier());
    text.emplaceInst<nasm::Mov>(dest, val);
    return;
  }
  if (auto global = ir::dycGlobalReg(p->getAddr())) {
    text.emplaceInst<nasm::Mov>(dest, makeLabelAddr(global));
    return;
  }

  text.emplaceInst<nasm::Mov>(
      dest, std::make_shared<nasm::MemoryAddr>(ctx.getAddr(irAddr)));
}

void genCall(nasm::Section &text, Context &ctx,
             const std::shared_ptr<ir::Call> &p) {
  static const std::vector<std::shared_ptr<nasm::Register>> ParamRegs = {
      nasm::rdi(), nasm::rsi(), nasm::rdx(),
      nasm::rcx(), nasm::r8(),  nasm::r9()};
  auto rspCopy = ctx.newVirtualReg();
  text.emplaceInst<nasm::Mov>(rspCopy, nasm::rsp());

  for (std::size_t i = 0; i < 6 && i < p->getArgs().size(); ++i) {
    text.emplaceInst<nasm::Mov>(ParamRegs[i], ctx.getAddr(p->getArgs()[i]));
  }
  for (int i = p->getArgs().size() - 1; i >= 6; --i) {
    auto tmp = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(tmp, ctx.getAddr(p->getArgs()[i]));
    text.emplaceInst<nasm::Push>(tmp);
  }

  text.emplaceInst<nasm::Call>(renameIdentifier(p->getFuncName()));
  if (p->getDest()) {
    auto retVal = ctx.getAddr(p->getDest());
    text.emplaceInst<nasm::Mov>(retVal, nasm::rax());
  }

  if (p->getArgs().size() > 6) {
    text.emplaceInst<nasm::Mov>(nasm::rsp(), rspCopy);
  }
}

bool genRelation(nasm::Section &text, Context &ctx,
                 const std::shared_ptr<ir::RelationInst> &p,
                 const std::shared_ptr<ir::IRInst> &nextInst,
                 std::size_t nextBB) {
  bool skipNext = true;
  auto br = ir::dyc<ir::Branch>(nextInst);
  if (!br)
    skipNext = false;
  auto irDest = p->getDest();
  auto condition = ir::dycLocalReg(br->getCondition());
  if (!condition || condition->getIdentifier() != irDest->getIdentifier())
    skipNext = false;
  if (ctx.getUses(irDest).size() > 1)
    skipNext = false;

  auto lhs = ctx.getAddr(p->getLhs());
  if (!nasm::dyc<nasm::Register>(lhs)) {
    lhs = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(lhs, ctx.getAddr(p->getLhs()));
  }
  auto rhs = ctx.getAddr(p->getRhs());
  assert(!nasm::dyc<nasm::LabelAddr>(rhs));
  text.emplaceInst<nasm::Cmp>(lhs, rhs);

  if (skipNext) {
    // Note that the values of this map is the counterpart of the key since it
    // is used to jump to the else-branch
    static const SmallMap<ir::RelationInst::OpType, nasm::CJump::OpType> opMap{
        {ir::RelationInst::Eq, nasm::CJump::Ne},
        {ir::RelationInst::Ne, nasm::CJump::Ez},
        {ir::RelationInst::Lt, nasm::CJump::Ge},
        {ir::RelationInst::Le, nasm::CJump::Gt},
        {ir::RelationInst::Gt, nasm::CJump::Le},
        {ir::RelationInst::Ge, nasm::CJump::Lt},
    };

    text.emplaceInst<nasm::CJump>(
        opMap.at(p->getOp()),
        std::make_shared<nasm::Label>(".L" +
                                      std::to_string(br->getElse()->getID())));
    if (br->getThen()->getID() != nextBB)
      text.emplaceInst<nasm::Jmp>(std::make_shared<nasm::Label>(
          ".L" + std::to_string(br->getThen()->getID())));
    return true;
  }

  auto dest = nasm::dyc<nasm::Register>(ctx.getAddr(p->getDest()));
  text.emplaceInst<nasm::Mov>(dest, std::make_shared<nasm::NumericConstant>(0));
  static const SmallMap<ir::RelationInst::OpType, nasm::Set::OpType> mp{
      {ir::RelationInst::Eq, nasm::Set::Eq},
      {ir::RelationInst::Ne, nasm::Set::Ne},
      {ir::RelationInst::Lt, nasm::Set::Lt},
      {ir::RelationInst::Le, nasm::Set::Le},
      {ir::RelationInst::Gt, nasm::Set::Gt},
      {ir::RelationInst::Ge, nasm::Set::Ge},
  };
  text.emplaceInst<nasm::Mov>(nasm::rax(),
                              std::make_shared<nasm::NumericConstant>(0));
  text.emplaceInst<nasm::Set>(mp.at(p->getOp()), nasm::rax());
  text.emplaceInst<nasm::Mov>(dest, nasm::rax());
  return false;
}

// return whether the next instruction should be skipped
bool genInstruction(nasm::Section &text, Context &ctx,
                    const std::shared_ptr<ir::IRInst> &inst,
                    const std::size_t nextBB,
                    const std::shared_ptr<ir::IRInst> &nextInst) {
  if (ir::dyc<ir::Comment>(inst) || ir::dyc<ir::AttachedComment>(inst)) {
    return false;
  }

  if (auto p = ir::dyc<ir::Alloca>(inst)) {
    ctx.addStackVar(p->getDest()->getIdentifier());
    return false;
  }
  if (auto p = ir::dyc<ir::Jump>(inst)) {
    text.emplaceInst<nasm::Jmp>(std::make_shared<nasm::Label>(
        ".L" + std::to_string(p->getLabel()->getID())));
    return false;
  }
  if (auto p = ir::dyc<ir::Store>(inst)) {
    genStore(text, ctx, p);
    return false;
  }
  if (auto p = ir::dyc<ir::Load>(inst)) {
    genLoad(text, ctx, p);
    return false;
  }
  if (auto p = ir::dyc<ir::ArithBinaryInst>(inst)) {
    if (p->getOp() == ir::ArithBinaryInst::Mod ||
        p->getOp() == ir::ArithBinaryInst::Div) {
      auto raxCopy = ctx.newVirtualReg(), rdxCopy = ctx.newVirtualReg();
      text.emplaceInst<nasm::Mov>(rdxCopy, nasm::rdx());
      text.emplaceInst<nasm::Mov>(raxCopy, nasm::rax());

      auto lhs = ctx.getAddr(p->getLhs());
      text.emplaceInst<nasm::Mov>(nasm::rax(), lhs);
      // text.emplaceInst<nasm::Cqo>();
      text.emplaceInst<nasm::Mov>(nasm::rdx(),
                                  std::make_shared<nasm::NumericConstant>(0));

      auto divisor = ctx.newVirtualReg();
      text.emplaceInst<nasm::Mov>(divisor, ctx.getAddr(p->getRhs()));
      text.emplaceInst<nasm::IDiv>(divisor);

      auto dest = ctx.getAddr(p->getDest());
      if (p->getOp() == ir::ArithBinaryInst::Mod)
        text.emplaceInst<nasm::Mov>(dest, nasm::rdx());
      else
        text.emplaceInst<nasm::Mov>(dest, nasm::rax());
      text.emplaceInst<nasm::Mov>(nasm::rdx(), rdxCopy);
      text.emplaceInst<nasm::Mov>(nasm::rax(), raxCopy);
      return false;
    }
    if (p->getOp() == ir::ArithBinaryInst::Shr ||
        p->getOp() == ir::ArithBinaryInst::Shl) {
      auto rcxCopy = ctx.newVirtualReg();
      text.emplaceInst<nasm::Mov>(rcxCopy, nasm::rcx());
      text.emplaceInst<nasm::Mov>(nasm::rcx(), ctx.getAddr(p->getRhs()));
      auto dest = ctx.getAddr(p->getDest());
      text.emplaceInst<nasm::Mov>(dest, ctx.getAddr(p->getLhs()));
      text.emplaceInst<nasm::BinaryInst>(p->getOp() == ir::ArithBinaryInst::Shr
                                             ? nasm::BinaryInst::Sar
                                             : nasm::BinaryInst::Sal,
                                         dest, nasm::rcx());
      text.emplaceInst<nasm::Mov>(nasm::rcx(), rcxCopy);
      return false;
    }

    static const SmallMap<ir::ArithBinaryInst::OpType, nasm::BinaryInst::OpType>
        opMp{
            {ir::ArithBinaryInst::Add, nasm::BinaryInst::Add},
            {ir::ArithBinaryInst::Sub, nasm::BinaryInst::Sub},
            {ir::ArithBinaryInst::Mul, nasm::BinaryInst::Mul},
            {ir::ArithBinaryInst::BitAnd, nasm::BinaryInst::BitAnd},
            {ir::ArithBinaryInst::BitOr, nasm::BinaryInst::BitOr},
            {ir::ArithBinaryInst::Xor, nasm::BinaryInst::Xor},
        };
    auto dest = ctx.getAddr(p->getDest());
    text.emplaceInst<nasm::Mov>(dest, ctx.getAddr(p->getLhs()));
    text.emplaceInst<nasm::BinaryInst>(opMp.at(p->getOp()), dest,
                                       ctx.getAddr(p->getRhs()));
    return false;
  }
  if (auto p = ir::dyc<ir::RelationInst>(inst)) {
    return genRelation(text, ctx, p, nextInst, nextBB);
  }
  if (auto p = ir::dyc<ir::Branch>(inst)) {
    auto lhs = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(lhs, ctx.getAddr(p->getCondition()));
    text.emplaceInst<nasm::Cmp>(lhs,
                                std::make_shared<nasm::NumericConstant>(0));
    text.emplaceInst<nasm::CJump>(
        nasm::CJump::Ez, std::make_shared<nasm::Label>(
                             ".L" + std::to_string(p->getElse()->getID())));
    if (p->getThen()->getID() != nextBB)
      text.emplaceInst<nasm::Jmp>(std::make_shared<nasm::Label>(
          ".L" + std::to_string(p->getThen()->getID())));
    return false;
  }
  if (auto p = ir::dyc<ir::Ret>(inst)) {
    if (p->getVal()) {
      auto val = ctx.getAddr(p->getVal());
      text.emplaceInst<nasm::Mov>(nasm::rax(), val);
    }
    for (auto &reg : CalleeSave) {
      text.emplaceInst<nasm::Mov>(reg, ctx.getPRegAddr(reg));
    }
    text.emplaceInst<nasm::Leave>();
    text.emplaceInst<nasm::Ret>();
    return false;
  }
  if (auto p = ir::dyc<ir::Call>(inst)) {
    genCall(text, ctx, p);
    return false;
  }
  if (auto p = ir::dyc<ir::Malloc>(inst)) {
    genCall(text, ctx,
            std::make_shared<ir::Call>(p->getDest(), "malloc", p->getSize()));
    return false;
  }
  if (auto p = ir::dyc<ir::ArithUnaryInst>(inst)) {
    auto dest = nasm::dyc<nasm::Register>(ctx.getAddr(p->getDest()));
    assert(dest);
    text.emplaceInst<nasm::Mov>(dest, ctx.getAddr(p->getOperand()));
    if (p->getOp() == ir::ArithUnaryInst::Neg)
      text.emplaceInst<nasm::UnaryInst>(nasm::UnaryInst::Neg, dest);
    else
      text.emplaceInst<nasm::UnaryInst>(nasm::UnaryInst::Not, dest);
    return false;
  }
  assert(false);
}

void genBasicBlock(nasm::Section &text, Context &ctx, const ir::BasicBlock &bb,
                   const std::size_t nextBB) {
  text.labelThisLine(".L" + std::to_string(bb.getLabelID()));
  for (auto iter = bb.getInsts().begin(), end = bb.getInsts().end();
       iter != end; ++iter) {
    auto next = iter;
    next++;
    bool skipNext =
        genInstruction(text, ctx, *iter, nextBB, next == end ? nullptr : *next);
    if (skipNext) {
      ++iter;
      if (iter == end)
        break;
    }
  }
}

void genFunction(nasm::Section &text, Context &ctx,
                 const ir::FunctionModule &func) {
  text.labelThisLine(renameIdentifier(func.getIdentifier()));
  text.emplaceInst<nasm::Push>(nasm::rbp());
  text.emplaceInst<nasm::Mov>(nasm::rbp(), nasm::rsp());

  for (auto &reg : CalleeSave) {
    if (reg->getIdentifier() == "rbp") {
      ctx.setPRegAddr(reg, reg);
      continue;
    }
    auto bak = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(bak, reg);
    ctx.setPRegAddr(reg, bak);
  }

  static const std::vector<std::shared_ptr<nasm::Register>> ParamRegs = {
      nasm::rdi(), nasm::rsi(), nasm::rdx(),
      nasm::rcx(), nasm::r8(),  nasm::r9()};
  for (std::size_t i = 0; i < 6 && i < func.getArgs().size(); ++i) {
    auto reg = std::make_shared<ir::Reg>(std::to_string(i));
    text.emplaceInst<nasm::Mov>(ctx.getAddr(reg), ParamRegs[i]);
  }
  for (std::size_t i = 6; i < func.getArgs().size(); ++i) {
    auto dest = ctx.getAddr(std::make_shared<ir::Reg>(std::to_string(i)));
    text.emplaceInst<nasm::Mov>(
        dest, std::make_shared<nasm::MemoryAddr>(nasm::rbp(), (i - 4) * 8));
  }

  std::vector<std::shared_ptr<ir::Reg>> globalVars;
  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto operands = ir::getOperandsUsed(inst);
      for (auto &operand : operands) {
        auto reg = ir::dycGlobalReg(operand);
        if (reg)
          globalVars.emplace_back(reg);
      }
    }
  }
  for (auto &reg : globalVars) {
    auto labelAddr =
        std::make_shared<nasm::LabelAddr>("L" + reg->getIdentifier());
    auto addrReg = ctx.newVirtualReg();
    text.emplaceInst<nasm::Lea>(addrReg, labelAddr);
    ctx.setIrRegAddr(reg, addrReg);
  }

  for (auto iter = func.getBBs().begin(), end = func.getBBs().end();
       iter != end; ++iter) {
    auto next = iter;
    next++;
    genBasicBlock(text, ctx, *iter,
                  next == end ? (std::size_t)-1 : next->getLabelID());
  }
}

} // namespace
} // namespace mocker

namespace mocker {
namespace {

nasm::Section genDataSection(const ir::Module &irModule) {
  nasm::Section res(".data");
  for (auto &var : irModule.getGlobalVars()) {
    res.emplaceLine<nasm::Db>("L" + var.getLabel(), var.getData());
  }
  return res;
}

nasm::Section genBssSection(const ir::Module &irModule) {
  nasm::Section res(".bss");
  return res;
}

nasm::Section genTextSection(const ir::Module &irModule) {
  nasm::Section text(".text");
  for (auto &kv : irModule.getFuncs()) {
    auto &func = kv.second;
    if (func.isExternalFunc())
      continue;
    Context ctx(text);
    ctx.init(func);
    genFunction(text, ctx, func);
  }
  return text;
}

} // namespace
} // namespace mocker

namespace mocker {

nasm::Module runNaiveInstructionSelection(const ir::Module &irModule) {
  nasm::Module res;

  // directives
  res.emplaceDirective<nasm::Default>("rel");
  res.emplaceDirective<nasm::Global>("main");
  res.emplaceDirective<nasm::Extern>("malloc");
  for (auto &kv : irModule.getFuncs()) {
    if (kv.second.isExternalFunc())
      res.emplaceDirective<nasm::Extern>(renameIdentifier(kv.first));
  }

  res.newSection(".data") = genDataSection(irModule);
  res.newSection(".bss") = genBssSection(irModule);
  res.newSection(".text") = genTextSection(irModule);

  return res;
}

} // namespace mocker
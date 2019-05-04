#include "instruction_selection.h"

#include "ir/helper.h"
#include "optim/analysis/defuse.h"
#include "small_map.h"
#include "vreg_assignment.h"

namespace mocker {
namespace {

const std::vector<std::shared_ptr<nasm::Register>> CalleeSave = {
    nasm::rbp(), nasm::rbx(), nasm::r12(),
    nasm::r13(), nasm::r14(), nasm::r15()};

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

std::shared_ptr<nasm::LabelAddr>
makeLabelAddr(const std::shared_ptr<ir::Reg> &reg) {
  return std::make_shared<nasm::LabelAddr>("L" + reg->getIdentifier());
}

class FuncSelectionContext {
public:
  void init(const ir::FunctionModule &func) {
    irRegAddr.init(func);
    defUse.init(func);
    useDef.init(func);
  }

  std::shared_ptr<nasm::Register> newVirtualReg(const std::string &hint = "") {
    return std::make_shared<nasm::Register>("v" + hint + "_" +
                                            std::to_string(vRegCnt++));
  }

  void setPRegAddr(const std::shared_ptr<nasm::Register> &pReg,
                   const std::shared_ptr<nasm::Register> &vReg) {
    pRegAddr[pReg] = vReg;
  }

  const std::shared_ptr<nasm::Register> &
  getPRegAddr(const std::shared_ptr<nasm::Register> &reg) const {
    return pRegAddr.at(reg);
  }

  void setIrRegAddr(const std::shared_ptr<ir::Reg> &irReg,
                    const std::shared_ptr<nasm::Register> &vReg) {
    irRegAddr.set(irReg, vReg);
  }

  std::shared_ptr<nasm::Addr>
  getIrAddr(const std::shared_ptr<ir::Addr> &addr) const {
    if (auto p = ir::dyc<ir::IntLiteral>(addr)) {
      return std::make_shared<nasm::NumericConstant>(p->getVal());
    }
    auto reg = ir::dyc<ir::Reg>(addr);
    if (reg->getIdentifier() == ".phi_nan")
      return std::make_shared<nasm::NumericConstant>(0);
    assert(reg);
    return irRegAddr.getVReg(reg);
  }

  std::shared_ptr<nasm::MemoryAddr>
  buildMemAddr(const std::shared_ptr<ir::Reg> &addr) const {
    // TODO
    return std::make_shared<nasm::MemoryAddr>(getIrAddr(addr));
  }

  const std::vector<typename DefUseChain::Use>
  getUses(const std::shared_ptr<ir::Reg> &reg) const {
    return defUse.getUses(reg);
  }

private:
  VRegAssignment irRegAddr;
  DefUseChain defUse;
  UseDefChain useDef;

  std::size_t vRegCnt = 0;
  nasm::RegMap<std::shared_ptr<nasm::Register>> pRegAddr;
};

} // namespace

namespace {

void genStore(nasm::Section &text, FuncSelectionContext &ctx,
              const std::shared_ptr<ir::Store> &p) {
  auto val = ctx.getIrAddr(p->getVal());
  auto irAddr = ir::dyc<ir::Reg>(p->getAddr());
  assert(irAddr);
  if (auto global = ir::dycGlobalReg(irAddr)) {
    text.emplaceInst<nasm::Mov>(makeLabelAddr(global), val);
    return;
  }
  text.emplaceInst<nasm::Mov>(ctx.buildMemAddr(irAddr), val);
}

void genLoad(nasm::Section &text, FuncSelectionContext &ctx,
             const std::shared_ptr<ir::Load> &p) {
  auto dest = ctx.getIrAddr(p->getDest());
  auto irAddr = ir::dyc<ir::Reg>(p->getAddr());
  assert(irAddr);
  if (auto global = ir::dycGlobalReg(p->getAddr())) {
    text.emplaceInst<nasm::Mov>(dest, makeLabelAddr(global));
    return;
  }
  text.emplaceInst<nasm::Mov>(dest, ctx.buildMemAddr(irAddr));
}

void genArithBinary(nasm::Section &text, FuncSelectionContext &ctx,
                    const std::shared_ptr<ir::ArithBinaryInst> &inst) {
  if (inst->getOp() == ir::ArithBinaryInst::Mod ||
      inst->getOp() == ir::ArithBinaryInst::Div) {
    auto raxCopy = ctx.newVirtualReg(), rdxCopy = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(rdxCopy, nasm::rdx());
    text.emplaceInst<nasm::Mov>(raxCopy, nasm::rax());

    auto lhs = ctx.getIrAddr(inst->getLhs());
    text.emplaceInst<nasm::Mov>(nasm::rax(), lhs);
    // text.emplaceInst<nasm::Cqo>();
    text.emplaceInst<nasm::Mov>(nasm::rdx(),
                                std::make_shared<nasm::NumericConstant>(0));

    auto divisor = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(divisor, ctx.getIrAddr(inst->getRhs()));
    text.emplaceInst<nasm::IDiv>(divisor);

    auto dest = ctx.getIrAddr(inst->getDest());
    if (inst->getOp() == ir::ArithBinaryInst::Mod)
      text.emplaceInst<nasm::Mov>(dest, nasm::rdx());
    else
      text.emplaceInst<nasm::Mov>(dest, nasm::rax());
    text.emplaceInst<nasm::Mov>(nasm::rdx(), rdxCopy);
    text.emplaceInst<nasm::Mov>(nasm::rax(), raxCopy);
    return;
  }

  if (inst->getOp() == ir::ArithBinaryInst::Shr ||
      inst->getOp() == ir::ArithBinaryInst::Shl) {
    auto dest = ctx.getIrAddr(inst->getDest());
    auto lhs = ctx.getIrAddr(inst->getLhs());
    text.emplaceInst<nasm::Mov>(dest, lhs);
    auto rhs = ctx.getIrAddr(inst->getRhs());

    if (nasm::dyc<nasm::NumericConstant>(rhs)) {
      text.emplaceInst<nasm::BinaryInst>(
          inst->getOp() == ir::ArithBinaryInst::Shr ? nasm::BinaryInst::Sar
                                                    : nasm::BinaryInst::Sal,
          dest, rhs);
      return;
    }

    auto rcxCopy = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(rcxCopy, nasm::rcx());
    text.emplaceInst<nasm::Mov>(nasm::rcx(), rhs);
    text.emplaceInst<nasm::BinaryInst>(inst->getOp() == ir::ArithBinaryInst::Shr
                                           ? nasm::BinaryInst::Sar
                                           : nasm::BinaryInst::Sal,
                                       dest, nasm::rcx());
    text.emplaceInst<nasm::Mov>(nasm::rcx(), rcxCopy);
    return;
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
  auto dest = ctx.getIrAddr(inst->getDest());
  text.emplaceInst<nasm::Mov>(dest, ctx.getIrAddr(inst->getLhs()));
  text.emplaceInst<nasm::BinaryInst>(opMp.at(inst->getOp()), dest,
                                     ctx.getIrAddr(inst->getRhs()));
}

bool genRelation(nasm::Section &text, FuncSelectionContext &ctx,
                 const std::shared_ptr<ir::RelationInst> &p,
                 const std::shared_ptr<ir::IRInst> &nextInst,
                 std::size_t nextBB) {
  bool skipNext = true;
  while (true) {
    auto br = ir::dyc<ir::Branch>(nextInst);
    if (!br) {
      skipNext = false;
      break;
    }
    auto irDest = p->getDest();
    auto condition = ir::dycLocalReg(br->getCondition());
    if (!condition || condition->getIdentifier() != irDest->getIdentifier() ||
        (nextBB != br->getThen()->getID() && nextBB != br->getElse()->getID())) {
      skipNext = false;
      break;
    }
    if (ctx.getUses(irDest).size() > 1) {
      skipNext = false;
      break;
    }
    break;
  }

  auto lhs = ctx.getIrAddr(p->getLhs());
  if (!nasm::dyc<nasm::Register>(lhs)) {
    lhs = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(lhs, ctx.getIrAddr(p->getLhs()));
  }
  auto rhs = ctx.getIrAddr(p->getRhs());
  assert(!nasm::dyc<nasm::LabelAddr>(rhs));
  text.emplaceInst<nasm::Cmp>(lhs, rhs);

  if (skipNext) {
    static const SmallMap<ir::RelationInst::OpType, nasm::CJump::OpType> Jump2ThenMap{
        {ir::RelationInst::Eq, nasm::CJump::Ez},
        {ir::RelationInst::Ne, nasm::CJump::Ne},
        {ir::RelationInst::Lt, nasm::CJump::Lt},
        {ir::RelationInst::Le, nasm::CJump::Le},
        {ir::RelationInst::Gt, nasm::CJump::Gt},
        {ir::RelationInst::Ge, nasm::CJump::Ge},
    };
    static const SmallMap<ir::RelationInst::OpType, nasm::CJump::OpType> Jump2ElseMap{
        {ir::RelationInst::Eq, nasm::CJump::Ne},
        {ir::RelationInst::Ne, nasm::CJump::Ez},
        {ir::RelationInst::Lt, nasm::CJump::Ge},
        {ir::RelationInst::Le, nasm::CJump::Gt},
        {ir::RelationInst::Gt, nasm::CJump::Le},
        {ir::RelationInst::Ge, nasm::CJump::Lt},
    };

    auto br = ir::dyc<ir::Branch>(nextInst);
    if (br->getThen()->getID() == nextBB) {  // fall through to then
      text.emplaceInst<nasm::CJump>(
          Jump2ElseMap.at(p->getOp()),
          std::make_shared<nasm::Label>(".L" +
              std::to_string(br->getElse()->getID())));
      return true;
    }
    // fall through to else
    text.emplaceInst<nasm::CJump>(
        Jump2ThenMap.at(p->getOp()),
        std::make_shared<nasm::Label>(".L" +
            std::to_string(br->getThen()->getID())));
    return true;
  }

  auto dest = nasm::dyc<nasm::Register>(ctx.getIrAddr(p->getDest()));
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

void genBranch(nasm::Section &text, FuncSelectionContext &ctx,
               const std::shared_ptr<ir::Branch> &inst, std::size_t nextBB) {
  auto lhs = ctx.newVirtualReg();
  text.emplaceInst<nasm::Mov>(lhs, ctx.getIrAddr(inst->getCondition()));
  text.emplaceInst<nasm::Cmp>(lhs, std::make_shared<nasm::NumericConstant>(0));

  if (inst->getThen()->getID() == nextBB) {
    text.emplaceInst<nasm::CJump>(
        nasm::CJump::Ez, std::make_shared<nasm::Label>(
            ".L" + std::to_string(inst->getElse()->getID())));
    return;
  }
  if (inst->getElse()->getID() == nextBB) {
    text.emplaceInst<nasm::CJump>(
        nasm::CJump::Ne, std::make_shared<nasm::Label>(
            ".L" + std::to_string(inst->getThen()->getID())));
    return;
  }

  text.emplaceInst<nasm::CJump>(
      nasm::CJump::Ez, std::make_shared<nasm::Label>(
                           ".L" + std::to_string(inst->getElse()->getID())));
  text.emplaceInst<nasm::Jmp>(std::make_shared<nasm::Label>(
      ".L" + std::to_string(inst->getThen()->getID())));
}

void genCall(nasm::Section &text, FuncSelectionContext &ctx,
             const std::shared_ptr<ir::Call> &p) {
  static const std::vector<std::shared_ptr<nasm::Register>> ParamRegs = {
      nasm::rdi(), nasm::rsi(), nasm::rdx(),
      nasm::rcx(), nasm::r8(),  nasm::r9()};
  auto rspCopy = ctx.newVirtualReg();
  text.emplaceInst<nasm::Mov>(rspCopy, nasm::rsp());

  for (std::size_t i = 0; i < 6 && i < p->getArgs().size(); ++i) {
    text.emplaceInst<nasm::Mov>(ParamRegs[i], ctx.getIrAddr(p->getArgs()[i]));
  }
  for (int i = p->getArgs().size() - 1; i >= 6; --i) {
    auto tmp = ctx.newVirtualReg();
    text.emplaceInst<nasm::Mov>(tmp, ctx.getIrAddr(p->getArgs()[i]));
    text.emplaceInst<nasm::Push>(tmp);
  }

  text.emplaceInst<nasm::Call>(renameIdentifier(p->getFuncName()));
  if (p->getDest()) {
    auto retVal = ctx.getIrAddr(p->getDest());
    text.emplaceInst<nasm::Mov>(retVal, nasm::rax());
  }

  if (p->getArgs().size() > 6) {
    text.emplaceInst<nasm::Mov>(nasm::rsp(), rspCopy);
  }
}

} // namespace

namespace {

ir::InstListIter genInst(nasm::Section &text, FuncSelectionContext &ctx,
                         const ir::FunctionModule &func,
                         ir::BasicBlockList::const_iterator curBBIter,
                         ir::BasicBlockList::const_iterator bbIterEnd,
                         ir::InstListIter curInstIter) {
  auto &inst = *curInstIter;
  auto nextIter = curInstIter;
  ++nextIter;
  auto nextBBIter = curBBIter;
  ++nextBBIter;

  if (ir::dyc<ir::Comment>(inst) || ir::dyc<ir::AttachedComment>(inst))
    return nextIter;

  if (ir::dyc<ir::Phi>(inst))
    return nextIter;

  if (auto p = ir::dyc<ir::Assign>(inst)) {
    text.emplaceInst<nasm::Mov>(ctx.getIrAddr(p->getDest()),
                                ctx.getIrAddr(p->getOperand()));
    return nextIter;
  }

  if (auto p = ir::dyc<ir::Jump>(inst)) {
    if (nextBBIter == bbIterEnd ||
        nextBBIter->getLabelID() != p->getLabel()->getID())
      text.emplaceInst<nasm::Jmp>(std::make_shared<nasm::Label>(
          ".L" + std::to_string(p->getLabel()->getID())));
    return nextIter;
  }

  if (auto p = ir::dyc<ir::Store>(inst)) {
    genStore(text, ctx, p);
    return nextIter;
  }
  if (auto p = ir::dyc<ir::Load>(inst)) {
    genLoad(text, ctx, p);
    return nextIter;
  }

  if (auto p = ir::dyc<ir::ArithBinaryInst>(inst)) {
    genArithBinary(text, ctx, p);
    return nextIter;
  }

  if (auto p = ir::dyc<ir::RelationInst>(inst)) {
    auto skip =
        genRelation(text, ctx, p, *nextIter,
                    nextBBIter == bbIterEnd ? -1 : nextBBIter->getLabelID());
    if (skip)
      ++nextIter;
    return nextIter;
  }

  if (auto p = ir::dyc<ir::Branch>(inst)) {
    genBranch(text, ctx, p,
              nextBBIter == bbIterEnd ? -1 : nextBBIter->getLabelID());
    return nextIter;
  }

  if (auto p = ir::dyc<ir::Ret>(inst)) {
    if (p->getVal()) {
      auto val = ctx.getIrAddr(p->getVal());
      text.emplaceInst<nasm::Mov>(nasm::rax(), val);
    }
    for (auto &reg : CalleeSave) {
      text.emplaceInst<nasm::Mov>(reg, ctx.getPRegAddr(reg));
    }
    text.emplaceInst<nasm::Leave>();
    text.emplaceInst<nasm::Ret>();
    return nextIter;
  }

  if (auto p = ir::dyc<ir::Call>(inst)) {
    genCall(text, ctx, p);
    return nextIter;
  }
  if (auto p = ir::dyc<ir::Malloc>(inst)) {
    genCall(text, ctx,
            std::make_shared<ir::Call>(p->getDest(), "__alloc", p->getSize()));
    return nextIter;
  }

  if (auto p = ir::dyc<ir::ArithUnaryInst>(inst)) {
    auto dest = nasm::dyc<nasm::Register>(ctx.getIrAddr(p->getDest()));
    assert(dest);
    text.emplaceInst<nasm::Mov>(dest, ctx.getIrAddr(p->getOperand()));
    if (p->getOp() == ir::ArithUnaryInst::Neg)
      text.emplaceInst<nasm::UnaryInst>(nasm::UnaryInst::Neg, dest);
    else
      text.emplaceInst<nasm::UnaryInst>(nasm::UnaryInst::Not, dest);
    return nextIter;
  }
  assert(false);
}

void genBasicBlock(nasm::Section &text, FuncSelectionContext &ctx,
                   const ir::FunctionModule &func,
                   ir::BasicBlockList::const_iterator curIter,
                   ir::BasicBlockList::const_iterator bbEnd) {
  auto &bb = *curIter;
  text.labelThisLine(".L" + std::to_string(bb.getLabelID()));
  for (auto iter = bb.getInsts().begin(), end = bb.getInsts().end();
       iter != end;) {
    iter = genInst(text, ctx, func, curIter, bbEnd, iter);
  }
}

void genFunction(nasm::Section &text, FuncSelectionContext &ctx,
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
    ctx.setIrRegAddr(reg, ctx.newVirtualReg(reg->getIdentifier()));
    text.emplaceInst<nasm::Mov>(ctx.getIrAddr(reg), ParamRegs[i]);
  }
  for (std::size_t i = 6; i < func.getArgs().size(); ++i) {
    auto reg = std::make_shared<ir::Reg>(std::to_string(i));
    auto dest = ctx.newVirtualReg(reg->getIdentifier());
    ctx.setIrRegAddr(reg, dest);
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
    genBasicBlock(text, ctx, func, iter, end);
  }
}

} // namespace

} // namespace mocker

namespace mocker {

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
    FuncSelectionContext ctx;
    ctx.init(func);
    genFunction(text, ctx, func);
  }
  return text;
}

nasm::Module runInstructionSelection(const ir::Module &irModule) {
  nasm::Module res;

  // directives
  res.emplaceDirective<nasm::Default>("rel");
  res.emplaceDirective<nasm::Global>("main");
  res.emplaceDirective<nasm::Extern>("malloc");
  res.emplaceDirective<nasm::Extern>("__alloc");
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
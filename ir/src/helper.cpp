#include "helper.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <unordered_set>

namespace mocker {
namespace ir {

std::shared_ptr<Addr> getDest(const std::shared_ptr<IRInst> &inst) {
  auto def = dyc<Definition>(inst);
  if (!def)
    return nullptr;
  return def->getDest();
}

const std::string &getLocalRegIdentifier(const std::shared_ptr<Addr> &addr) {
  auto p = dyc<LocalReg>(addr);
  assert(p);
  return p->getIdentifier();
}

std::vector<std::shared_ptr<Addr>>
getOperandsUsed(const std::shared_ptr<IRInst> &inst) {
  if (auto p = std::dynamic_pointer_cast<Assign>(inst))
    return {p->getOperand()};
  if (auto p = std::dynamic_pointer_cast<ArithUnaryInst>(inst))
    return {p->getOperand()};
  if (auto p = std::dynamic_pointer_cast<ArithBinaryInst>(inst))
    return {p->getLhs(), p->getRhs()};
  if (auto p = std::dynamic_pointer_cast<RelationInst>(inst))
    return {p->getLhs(), p->getRhs()};
  if (auto p = std::dynamic_pointer_cast<Store>(inst))
    return {p->getAddr(), p->getVal()};
  if (auto p = std::dynamic_pointer_cast<Load>(inst))
    return {p->getAddr()};
  if (auto p = std::dynamic_pointer_cast<Malloc>(inst))
    return {p->getSize()};
  if (auto p = std::dynamic_pointer_cast<StrCpy>(inst))
    return {p->getAddr()};
  if (auto p = std::dynamic_pointer_cast<Branch>(inst))
    return {p->getCondition()};
  if (auto p = std::dynamic_pointer_cast<Ret>(inst)) {
    if (auto v = p->getVal())
      return {v};
    return {};
  }

  if (auto p = std::dynamic_pointer_cast<Call>(inst))
    return p->getArgs();
  if (auto p = std::dynamic_pointer_cast<Phi>(inst)) {
    std::vector<std::shared_ptr<Addr>> res;
    for (auto &option : p->getOptions())
      res.emplace_back(option.first);
    return res;
  }
  return {};
}

template <class T, class... Args> std::shared_ptr<T> mkS(Args &&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

namespace {
std::shared_ptr<IRInst> copyWithReplacedDestAndOperands(
    const std::shared_ptr<ir::IRInst> &inst,
    const std::shared_ptr<ir::Addr> &dest,
    const std::vector<std::shared_ptr<ir::Addr>> &operands) {
  if (dyc<Deleted>(inst)) {
    assert(operands.empty());
    return mkS<Deleted>();
  }
  if (auto p = dyc<Comment>(inst)) {
    assert(operands.empty());
    return mkS<Comment>(p->getContent());
  }
  if (auto p = dyc<AttachedComment>(inst)) {
    assert(operands.empty());
    return mkS<AttachedComment>(p->getContent());
  }
  if (auto p = dyc<AllocVar>(inst)) {
    assert(operands.empty());
    return mkS<AllocVar>(dest);
  }
  if (auto p = dyc<Jump>(inst)) {
    assert(operands.empty());
    return mkS<Jump>(p->getLabel());
  }

  if (auto p = dyc<Assign>(inst)) {
    assert(operands.size() == 1);
    return mkS<Assign>(dest, operands[0]);
  }
  if (auto p = dyc<ArithUnaryInst>(inst)) {
    assert(operands.size() == 1);
    return mkS<ArithUnaryInst>(dest, p->getOp(), operands[0]);
  }
  if (auto p = dyc<ArithBinaryInst>(inst)) {
    assert(operands.size() == 2);
    return mkS<ArithBinaryInst>(dest, p->getOp(), operands[0], operands[1]);
  }
  if (auto p = dyc<RelationInst>(inst)) {
    assert(operands.size() == 2);
    return mkS<RelationInst>(dest, p->getOp(), operands[0], operands[1]);
  }
  if (auto p = dyc<Store>(inst)) {
    assert(operands.size() == 2);
    return mkS<Store>(operands[0], operands[1]);
  }
  if (auto p = dyc<Load>(inst)) {
    assert(operands.size() == 1);
    return mkS<Load>(dest, operands[0]);
  }
  if (auto p = dyc<Malloc>(inst)) {
    assert(operands.size() == 1);
    return mkS<Malloc>(dest, operands[0]);
  }
  if (auto p = dyc<StrCpy>(inst)) {
    assert(operands.size() == 1);
    return mkS<StrCpy>(operands[0], p->getData());
  }
  if (auto p = dyc<Branch>(inst)) {
    assert(operands.size() == 1);
    return mkS<Branch>(operands[0], p->getThen(), p->getElse());
  }
  if (auto p = dyc<Ret>(inst)) {
    assert(operands.size() <= 1);
    return mkS<Ret>(operands.empty() ? std::shared_ptr<Addr>(nullptr)
                                     : operands[0]);
  }
  if (auto p = dyc<Call>(inst)) {
    assert(operands.size() == p->getArgs().size());
    std::vector<std::shared_ptr<Addr>> args;
    args.reserve(operands.size());
    for (auto &arg : operands)
      args.emplace_back(arg);
    return mkS<Call>(dest, p->getFuncName(), std::move(args));
  }
  if (auto p = dyc<Phi>(inst)) {
    assert(operands.size() == p->getOptions().size());
    std::vector<Phi::Option> options;
    for (std::size_t i = 0; i < operands.size(); ++i) {
      auto val = operands[i];
      auto label = dyc<Label>(p->getOptions()[i].second);
      options.emplace_back(std::make_pair(val, label));
    }
    return mkS<Phi>(dest, std::move(options));
  }
  assert(false);
}
} // namespace

std::shared_ptr<IRInst> copyWithReplacedOperands(
    const std::shared_ptr<ir::IRInst> &inst,
    const std::vector<std::shared_ptr<ir::Addr>> &operands) {
  if (dyc<Deleted>(inst)) {
    assert(operands.empty());
    return mkS<Deleted>();
  }
  if (auto p = dyc<Comment>(inst)) {
    assert(operands.empty());
    return mkS<Comment>(p->getContent());
  }
  if (auto p = dyc<AttachedComment>(inst)) {
    assert(operands.empty());
    return mkS<AttachedComment>(p->getContent());
  }
  if (auto p = dyc<AllocVar>(inst)) {
    assert(operands.empty());
    return mkS<AllocVar>(p->getDest());
  }
  if (auto p = dyc<Jump>(inst)) {
    assert(operands.empty());
    return mkS<Jump>(p->getLabel());
  }

  if (auto p = dyc<Assign>(inst)) {
    assert(operands.size() == 1);
    return mkS<Assign>(p->getDest(), operands[0]);
  }
  if (auto p = dyc<ArithUnaryInst>(inst)) {
    assert(operands.size() == 1);
    return mkS<ArithUnaryInst>(p->getDest(), p->getOp(), operands[0]);
  }
  if (auto p = dyc<ArithBinaryInst>(inst)) {
    assert(operands.size() == 2);
    return mkS<ArithBinaryInst>(p->getDest(), p->getOp(), operands[0],
                                operands[1]);
  }
  if (auto p = dyc<RelationInst>(inst)) {
    assert(operands.size() == 2);
    return mkS<RelationInst>(p->getDest(), p->getOp(), operands[0],
                             operands[1]);
  }
  if (auto p = dyc<Store>(inst)) {
    assert(operands.size() == 2);
    return mkS<Store>(operands[0], operands[1]);
  }
  if (auto p = dyc<Load>(inst)) {
    assert(operands.size() == 1);
    return mkS<Load>(p->getDest(), operands[0]);
  }
  if (auto p = dyc<Malloc>(inst)) {
    assert(operands.size() == 1);
    return mkS<Malloc>(p->getDest(), operands[0]);
  }
  if (auto p = dyc<StrCpy>(inst)) {
    assert(operands.size() == 1);
    return mkS<StrCpy>(operands[0], p->getData());
  }
  if (auto p = dyc<Branch>(inst)) {
    assert(operands.size() == 1);
    return mkS<Branch>(operands[0], p->getThen(), p->getElse());
  }
  if (auto p = dyc<Ret>(inst)) {
    assert(operands.size() <= 1);
    return mkS<Ret>(operands.empty() ? std::shared_ptr<Addr>(nullptr)
                                     : operands[0]);
  }
  if (auto p = dyc<Call>(inst)) {
    assert(operands.size() == p->getArgs().size());
    std::vector<std::shared_ptr<Addr>> args;
    args.reserve(operands.size());
    for (auto &arg : operands)
      args.emplace_back(arg);
    return mkS<Call>(p->getDest(), p->getFuncName(), std::move(args));
  }
  if (auto p = dyc<Phi>(inst)) {
    assert(operands.size() == p->getOptions().size());
    std::vector<Phi::Option> options;
    for (std::size_t i = 0; i < operands.size(); ++i) {
      auto val = operands[i];
      auto label = dyc<Label>(p->getOptions()[i].second);
      options.emplace_back(std::make_pair(val, label));
    }
    return mkS<Phi>(p->getDest(), std::move(options));
  }
  assert(false);
}

std::shared_ptr<IRInst>
copyWithReplacedDest(const std::shared_ptr<ir::IRInst> &inst,
                     const std::shared_ptr<ir::Addr> &newDest) {
  return copyWithReplacedDestAndOperands(inst, newDest, getOperandsUsed(inst));
}

void verifyFuncModule(const ir::FunctionModule &func) {
  // check whether all blocks are terminated
  for (const auto &bb : func.getBBs())
    if (!bb.isCompleted())
      assert(false && "block not being terminated");

  // check whether all register are only defined once
  std::unordered_set<std::string> defined;
  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto dest = getDest(inst);
      if (!dest)
        continue;
      auto reg = dyc<ir::LocalReg>(dest);
      assert(reg);
      if (defined.find(reg->getIdentifier()) != defined.end())
        assert(false && "register with multiple definitions");
      defined.emplace(reg->getIdentifier());
    }
  }

  // check whether there exists phi-functions that are in the middle of a block
  for (const auto &bb : func.getBBs()) {
    auto iter = bb.getInsts().begin();
    while (ir::dyc<ir::Phi>(*iter))
      ++iter;
    if (!ir::dyc<ir::Terminator>(*iter)) {
      if (ir::dyc<ir::Phi>(*iter))
        assert(false && "misplaced phi-function");
      ++iter;
    }
  }

  // check the validation of phi-functions
  for (const auto &bb : func.getBBs()) {
    auto preds = func.getPredcessors(bb.getLabelID());
    std::sort(preds.begin(), preds.end());

    for (auto &inst : bb.getInsts()) {
      auto phi = dyc<ir::Phi>(inst);
      if (!phi)
        break;

      std::vector<std::size_t> sources;
      for (auto &option : phi->getOptions())
        sources.emplace_back(option.second->getID());
      std::sort(sources.begin(), sources.end());
      if (preds != sources)
        assert(false && "mismatched predecessors and phi-function options");
    }
  }
}

void verifyModule(const ir::Module &module) {
  for (auto &kv : module.getFuncs())
    verifyFuncModule(kv.second);
}

} // namespace ir
} // namespace mocker
#include "helper.h"

#include <cassert>
#include <helper.h>
#include <string>

namespace mocker {
namespace ir {

std::shared_ptr<const Addr> getDest(const std::shared_ptr<IRInst> &inst) {
  auto def = dyc<Definition>(inst);
  if (!def)
    return nullptr;
  return def->getDest();
}

const std::string &
getLocalRegIdentifier(const std::shared_ptr<const Addr> &addr) {
  auto p = cdyc<LocalReg>(addr);
  assert(p);
  return p->identifier;
}

std::vector<std::shared_ptr<const Addr>>
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
    std::vector<std::shared_ptr<const Addr>> res;
    for (auto &option : p->getOptions())
      res.emplace_back(option.first);
    return res;
  }
  return {};
}

template <class T, class... Args> std::shared_ptr<T> mkS(Args &&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

std::shared_ptr<Addr> copy(const std::shared_ptr<const ir::Addr> &addr) {
  if (auto p = cdyc<IntLiteral>(addr))
    return mkS<IntLiteral>(p->val);
  if (auto p = cdyc<LocalReg>(addr))
    return mkS<LocalReg>(p->identifier);
  if (auto p = cdyc<GlobalReg>(addr))
    return mkS<GlobalReg>(p->identifier);
  if (auto p = cdyc<Label>(addr))
    return mkS<Label>(p->id);
  return nullptr;
}

std::shared_ptr<IRInst> copy(const std::shared_ptr<ir::IRInst> &inst) {
  return copyWithReplacedOperands(inst, getOperandsUsed(inst));
}

// local
std::shared_ptr<IRInst> copyWithReplacedDestAndOperands(
    const std::shared_ptr<ir::IRInst> &inst,
    const std::shared_ptr<const ir::Addr> &dest,
    const std::vector<std::shared_ptr<const ir::Addr>> &operands) {
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
  if (auto p = dyc<Alloca>(inst)) {
    assert(operands.empty());
    return mkS<Alloca>(copy(dest), p->getSize());
  }
  if (auto p = dyc<SAlloc>(inst)) {
    assert(operands.empty());
    return mkS<SAlloc>(copy(dest), p->getSize());
  }
  if (auto p = dyc<Jump>(inst)) {
    assert(operands.empty());
    return mkS<Jump>(std::make_shared<Label>(p->getLabel()->id));
  }

  if (auto p = dyc<Assign>(inst)) {
    assert(operands.size() == 1);
    return mkS<Assign>(copy(dest), copy(operands[0]));
  }
  if (auto p = dyc<ArithUnaryInst>(inst)) {
    assert(operands.size() == 1);
    return mkS<ArithUnaryInst>(copy(dest), p->getOp(), copy(operands[0]));
  }
  if (auto p = dyc<ArithBinaryInst>(inst)) {
    assert(operands.size() == 2);
    return mkS<ArithBinaryInst>(copy(dest), p->getOp(), copy(operands[0]),
                                copy(operands[1]));
  }
  if (auto p = dyc<RelationInst>(inst)) {
    assert(operands.size() == 2);
    return mkS<RelationInst>(copy(dest), p->getOp(), copy(operands[0]),
                             copy(operands[1]));
  }
  if (auto p = dyc<Store>(inst)) {
    assert(operands.size() == 2);
    return mkS<Store>(copy(operands[0]), copy(operands[1]));
  }
  if (auto p = dyc<Load>(inst)) {
    assert(operands.size() == 1);
    return mkS<Load>(copy(dest), copy(operands[0]));
  }
  if (auto p = dyc<Malloc>(inst)) {
    assert(operands.size() == 1);
    return mkS<Malloc>(copy(dest), copy(operands[0]));
  }
  if (auto p = dyc<StrCpy>(inst)) {
    assert(operands.size() == 1);
    return mkS<StrCpy>(copy(operands[0]), p->getData());
  }
  if (auto p = dyc<Branch>(inst)) {
    assert(operands.size() == 1);
    return mkS<Branch>(copy(operands[0]), dyc<Label>(copy(p->getThen())),
                       dyc<Label>(copy(p->getElse())));
  }
  if (auto p = dyc<Ret>(inst)) {
    assert(operands.size() <= 1);
    return mkS<Ret>(operands.empty() ? std::shared_ptr<Addr>(nullptr)
                                     : copy(operands[0]));
  }
  if (auto p = dyc<Call>(inst)) {
    assert(operands.size() == p->getArgs().size());
    std::vector<std::shared_ptr<Addr>> args;
    args.reserve(operands.size());
    for (auto &arg : operands)
      args.emplace_back(copy(arg));
    return mkS<Call>(copy(dest), p->getFuncName(), std::move(args));
  }
  if (auto p = dyc<Phi>(inst)) {
    assert(operands.size() == p->getOptions().size());
    std::vector<Phi::Option> options;
    for (std::size_t i = 0; i < operands.size(); ++i) {
      auto val = copy(operands[i]);
      auto label = dyc<Label>(copy(p->getOptions()[i].second));
      options.emplace_back(std::make_pair(val, label));
    }
    return mkS<Phi>(copy(dest), std::move(options));
  }
  assert(false);
}

std::shared_ptr<IRInst> copyWithReplacedOperands(
    const std::shared_ptr<ir::IRInst> &inst,
    const std::vector<std::shared_ptr<const ir::Addr>> &operands) {
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
  if (auto p = dyc<Alloca>(inst)) {
    assert(operands.empty());
    return mkS<Alloca>(copy(p->getDest()), p->getSize());
  }
  if (auto p = dyc<SAlloc>(inst)) {
    assert(operands.empty());
    return mkS<SAlloc>(copy(p->getDest()), p->getSize());
  }
  if (auto p = dyc<Jump>(inst)) {
    assert(operands.empty());
    return mkS<Jump>(std::make_shared<Label>(p->getLabel()->id));
  }

  if (auto p = dyc<Assign>(inst)) {
    assert(operands.size() == 1);
    return mkS<Assign>(copy(p->getDest()), copy(operands[0]));
  }
  if (auto p = dyc<ArithUnaryInst>(inst)) {
    assert(operands.size() == 1);
    return mkS<ArithUnaryInst>(copy(p->getDest()), p->getOp(),
                               copy(operands[0]));
  }
  if (auto p = dyc<ArithBinaryInst>(inst)) {
    assert(operands.size() == 2);
    return mkS<ArithBinaryInst>(copy(p->getDest()), p->getOp(),
                                copy(operands[0]), copy(operands[1]));
  }
  if (auto p = dyc<RelationInst>(inst)) {
    assert(operands.size() == 2);
    return mkS<RelationInst>(copy(p->getDest()), p->getOp(), copy(operands[0]),
                             copy(operands[1]));
  }
  if (auto p = dyc<Store>(inst)) {
    assert(operands.size() == 2);
    return mkS<Store>(copy(operands[0]), copy(operands[1]));
  }
  if (auto p = dyc<Load>(inst)) {
    assert(operands.size() == 1);
    return mkS<Load>(copy(p->getDest()), copy(operands[0]));
  }
  if (auto p = dyc<Malloc>(inst)) {
    assert(operands.size() == 1);
    return mkS<Malloc>(copy(p->getDest()), copy(operands[0]));
  }
  if (auto p = dyc<StrCpy>(inst)) {
    assert(operands.size() == 1);
    return mkS<StrCpy>(copy(operands[0]), p->getData());
  }
  if (auto p = dyc<Branch>(inst)) {
    assert(operands.size() == 1);
    return mkS<Branch>(copy(operands[0]), dyc<Label>(copy(p->getThen())),
                       dyc<Label>(copy(p->getElse())));
  }
  if (auto p = dyc<Ret>(inst)) {
    assert(operands.size() <= 1);
    return mkS<Ret>(operands.empty() ? std::shared_ptr<Addr>(nullptr)
                                     : copy(operands[0]));
  }
  if (auto p = dyc<Call>(inst)) {
    assert(operands.size() == p->getArgs().size());
    std::vector<std::shared_ptr<Addr>> args;
    args.reserve(operands.size());
    for (auto &arg : operands)
      args.emplace_back(copy(arg));
    return mkS<Call>(copy(p->getDest()), p->getFuncName(), std::move(args));
  }
  if (auto p = dyc<Phi>(inst)) {
    assert(operands.size() == p->getOptions().size());
    std::vector<Phi::Option> options;
    for (std::size_t i = 0; i < operands.size(); ++i) {
      auto val = copy(operands[i]);
      auto label = dyc<Label>(copy(p->getOptions()[i].second));
      options.emplace_back(std::make_pair(val, label));
    }
    return mkS<Phi>(copy(p->getDest()), std::move(options));
  }
  assert(false);
}

std::shared_ptr<IRInst>
copyWithReplacedDest(const std::shared_ptr<ir::IRInst> &inst,
                     const std::shared_ptr<const ir::Addr> &newDest) {
  return copyWithReplacedDestAndOperands(inst, newDest, getOperandsUsed(inst));
}

void verifyFuncModule(const ir::FunctionModule &func) {
  for (const auto &bb : func.getBBs())
    if (!bb.isCompleted())
      std::terminate();
  // TODO
}

} // namespace ir
} // namespace mocker
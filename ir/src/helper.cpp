#include "helper.h"

#include <cassert>
#include <helper.h>
#include <string>

#define MOCKER_IR_GET_DEST(TYPE)                                               \
  if (auto p = std::dynamic_pointer_cast<TYPE>(inst))                          \
    return p->dest;

namespace mocker {
namespace ir {

std::shared_ptr<Addr> getDest(const std::shared_ptr<IRInst> &inst) {
  MOCKER_IR_GET_DEST(Assign);
  MOCKER_IR_GET_DEST(ArithUnaryInst);
  MOCKER_IR_GET_DEST(ArithBinaryInst);
  MOCKER_IR_GET_DEST(RelationInst);
  MOCKER_IR_GET_DEST(Load);
  MOCKER_IR_GET_DEST(Alloca);
  MOCKER_IR_GET_DEST(Malloc);
  MOCKER_IR_GET_DEST(SAlloc);
  MOCKER_IR_GET_DEST(Call);
  MOCKER_IR_GET_DEST(Phi);
  return nullptr;
}

const std::string &getLocalRegIdentifier(const std::shared_ptr<Addr> &addr) {
  auto p = std::dynamic_pointer_cast<LocalReg>(addr);
  assert(p);
  return p->identifier;
}

std::vector<std::shared_ptr<Addr>>
getOperandsUsed(const std::shared_ptr<IRInst> &inst) {
  std::vector<std::shared_ptr<Addr>> res;
  auto refs = getOperandUsedRef(inst);
  res.reserve(refs.size());
  for (auto &ref : refs)
    res.emplace_back(ref.get());
  return res;
}

std::vector<std::reference_wrapper<std::shared_ptr<Addr>>>
getOperandUsedRef(const std::shared_ptr<IRInst> &inst) {
  using ResType = std::vector<std::reference_wrapper<std::shared_ptr<Addr>>>;

  if (auto p = std::dynamic_pointer_cast<Assign>(inst))
    return {p->operand};
  if (auto p = std::dynamic_pointer_cast<ArithUnaryInst>(inst))
    return {p->operand};
  if (auto p = std::dynamic_pointer_cast<ArithBinaryInst>(inst))
    return {p->lhs, p->rhs};
  if (auto p = std::dynamic_pointer_cast<RelationInst>(inst))
    return {p->lhs, p->rhs};
  if (auto p = std::dynamic_pointer_cast<Store>(inst))
    return {p->dest, p->operand};
  if (auto p = std::dynamic_pointer_cast<Load>(inst))
    return {p->addr};
  if (auto p = std::dynamic_pointer_cast<Malloc>(inst))
    return {p->size};
  if (auto p = std::dynamic_pointer_cast<StrCpy>(inst))
    return {p->dest};
  if (auto p = std::dynamic_pointer_cast<Branch>(inst))
    return {p->condition};
  if (auto p = std::dynamic_pointer_cast<Ret>(inst)) {
    if (p->val)
      return {p->val};
    return {};
  }
  if (auto p = std::dynamic_pointer_cast<Call>(inst)) {
    ResType res;
    for (auto &arg : p->args)
      res.emplace_back(arg);
    return res;
  }
  if (auto p = std::dynamic_pointer_cast<Phi>(inst)) {
    ResType res;
    for (auto &option : p->options)
      res.emplace_back(option.first);
    return res;
  }
  return {};
}

} // namespace ir
} // namespace mocker
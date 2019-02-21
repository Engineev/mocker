#include "constant_propagation.h"

#include <cassert>
#include <iostream>

#include "helper.h"
#include "ir/helper.h"
#include "optional.h"

namespace mocker {

SparseSimpleConstantPropagation::SparseSimpleConstantPropagation(
    ir::FunctionModule &func)
    : FuncPass(func) {}

void SparseSimpleConstantPropagation::operator()() {
  buildInstDefineAndInstsUse();
  initialize();
  propagate();
  rewrite();
  removeDeletedInsts(func);
  std::cerr << "SSCP: Modify " << modificationCnt << " insts in "
            << func.getIdentifier() << std::endl;
}

void SparseSimpleConstantPropagation::buildInstDefineAndInstsUse() {
  auto updateInstsUse = [&](const std::shared_ptr<ir::IRInst> &inst) {
    auto operands = ir::getOperandsUsed(inst);
    for (auto &operand : operands) {
      if (auto p = dyc<ir::LocalReg>(operand))
        instsUse[p->identifier].emplace_back(inst);
    }
  };

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto dest = ir::getDest(inst);
      if (!dest)
        continue;
      auto destName = ir::getLocalRegIdentifier(dest);
      instDefine.emplace(destName, inst);
      updateInstsUse(inst);
    }
  }
}

void SparseSimpleConstantPropagation::initialize() {
  for (auto &kv : instDefine) {
    auto val = computeValue(kv.first);
    values[kv.first] = val;
    if (val.type == Value::Constant)
      workList.emplace(kv.first);
  }
}

void SparseSimpleConstantPropagation::propagate() {
  while (!workList.empty()) {
    auto name = workList.front();
    workList.pop();
    for (auto &inst : instsUse[name]) {
      auto dest = ir::getDest(inst);
      assert(dest);
      const auto &destName = ir::getLocalRegIdentifier(dest);
      if (values.at(destName).type == Value::Bottom)
        continue;
      auto newVal = computeValue(destName);
      auto &oldVal = values[destName];
      assert(!(oldVal.type == Value::Constant &&
               newVal.type == Value::Constant && newVal.val != oldVal.val));
      if (oldVal != newVal) {
        oldVal = newVal;
        workList.emplace(destName);
      }
    }
  }
}

void SparseSimpleConstantPropagation::rewrite() {
  auto rewriteIfPossible = [&](const std::shared_ptr<ir::IRInst> &inst)
      -> std::shared_ptr<ir::IRInst> {
    auto dest = dyc<ir::LocalReg>(ir::getDest(inst));
    if (dest) {
      auto destName = ir::getLocalRegIdentifier(dest);
      auto val = values[destName];
      if (val.type == Value::Constant) {
        // All the insts that use this value will be rewrite so that they use
        // the constant directly. This inst can therefore be deleted.
        ++modificationCnt;
        return std::make_shared<ir::Deleted>();
      }
    }

    // Rewrite the operands
    auto newInst = inst;
    auto operandRefs = ir::getOperandUsedRef(newInst);
    for (auto &operand : operandRefs) {
      auto reg = dyc<ir::LocalReg>(operand.get());
      if (!reg)
        continue;
      auto name = ir::getLocalRegIdentifier(reg);
      auto val = values[name];
      if (val.type == Value::Constant) {
        ++modificationCnt;
        operand.get() = std::make_shared<ir::IntLiteral>(val.val);
      }
    }

    return newInst;
  };

  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts())
      inst = rewriteIfPossible(inst);
  }
}

SparseSimpleConstantPropagation::Value
SparseSimpleConstantPropagation::computeValue(const std::string &destName) {
  auto inst = instDefine.at(destName);
  if (auto p = dyc<ir::Assign>(inst)) {
    return getValue(p->operand);
  }
  if (auto p = dyc<ir::ArithUnaryInst>(inst)) {
    if (auto v = dyc<ir::IntLiteral>(p->operand))
      return Value(p->op == ir::ArithUnaryInst::BitNot ? ~v->val : -v->val);
    if (!dyc<ir::LocalReg>(p->operand))
      return Value(Value::Bottom);
    auto operandV = values[ir::getLocalRegIdentifier(p->operand)];
    if (operandV.type == Value::Constant)
      return Value(p->op == ir::ArithUnaryInst::BitNot ? ~operandV.val
                                                       : -operandV.val);
    return operandV;
  }
  if (auto p = dyc<ir::ArithBinaryInst>(inst)) {
    auto compute = [](ir::ArithBinaryInst::OpType op, std::int64_t lhs,
                      std::int64_t rhs) -> std::int64_t {
      switch (op) {
      case ir::ArithBinaryInst::BitOr:
        return lhs | rhs;
      case ir::ArithBinaryInst::BitAnd:
        return lhs & rhs;
      case ir::ArithBinaryInst::Xor:
        return lhs ^ rhs;
      case ir::ArithBinaryInst::Shl:
        return lhs << rhs;
      case ir::ArithBinaryInst::Shr:
        return lhs >> rhs;
      case ir::ArithBinaryInst::Add:
        return lhs + rhs;
      case ir::ArithBinaryInst::Sub:
        return lhs - rhs;
      case ir::ArithBinaryInst::Mul:
        return lhs * rhs;
      case ir::ArithBinaryInst::Div:
        return lhs / rhs;
      case ir::ArithBinaryInst::Mod:
        return lhs % rhs;
      default:
        assert(false);
      }
    };
    auto lhsV = getValue(p->lhs), rhsV = getValue(p->rhs);
    if ((p->op == ir::ArithBinaryInst::Mul ||
         p->op == ir::ArithBinaryInst::BitAnd) &&
        ((lhsV.type == Value::Constant && lhsV.val == 0) ||
         (rhsV.type == Value::Constant && rhsV.val == 0)))
      return Value(0);

    if (lhsV.type == Value::Bottom || rhsV.type == Value::Bottom)
      return Value(Value::Bottom);
    if (lhsV.type == Value::Constant && rhsV.type == Value::Constant) {
      if ((p->op == ir::ArithBinaryInst::Div ||
           p->op == ir::ArithBinaryInst::Mod) &&
          rhsV.val == 0)
        return Value(Value::Bottom);
      return Value(compute(p->op, lhsV.val, rhsV.val));
    }
    return Value(Value::Top);
  }
  if (auto p = dyc<ir::RelationInst>(inst)) {
    auto compute = [](ir::RelationInst::OpType op, std::int64_t lhs,
                      std::int64_t rhs) -> std::int64_t {
      switch (op) {
      case ir::RelationInst::Eq:
        return lhs == rhs;
      case ir::RelationInst::Ne:
        return lhs != rhs;
      case ir::RelationInst::Lt:
        return lhs < rhs;
      case ir::RelationInst::Gt:
        return lhs > rhs;
      case ir::RelationInst::Le:
        return lhs <= rhs;
      case ir::RelationInst::Ge:
        return lhs >= rhs;
      default:
        assert(false);
      }
    };
    auto lhsV = getValue(p->lhs), rhsV = getValue(p->rhs);
    if (lhsV.type == Value::Bottom || rhsV.type == Value::Bottom)
      return Value(Value::Bottom);
    if (lhsV.type == Value::Constant && rhsV.type == Value::Constant)
      return Value(compute(p->op, lhsV.val, rhsV.val));
    return Value(Value::Top);
  }
  if (dyc<ir::Load>(inst) || dyc<ir::Alloca>(inst) || dyc<ir::Malloc>(inst) ||
      dyc<ir::SAlloc>(inst) || dyc<ir::Call>(inst))
    return Value(Value::Bottom);
  if (auto p = dyc<ir::Phi>(inst)) {
    Optional<std::int64_t> lastVal;
    for (auto &option : p->options) {
      if (auto reg = dyc<ir::LocalReg>(option.first)) {
        if (reg->identifier == ".phi_nan")
          continue;
      }
      auto optionV = getValue(option.first);
      if (optionV.type == Value::Bottom || optionV.type == Value::Top)
        return Value(optionV.type);
      if (optionV.type == Value::Constant) {
        if (!lastVal.hasValue()) {
          lastVal = Optional<std::int64_t>(optionV.val);
          continue;
        }
        if (lastVal.value() != optionV.val)
          return Value(Value::Bottom);
      }
    }
    assert(lastVal);
    return Value(lastVal.value());
  }
  // Store, StrCpy, Branch, Jump, Ret,
  assert(false);
}

SparseSimpleConstantPropagation::Value
SparseSimpleConstantPropagation::getValue(
    const std::shared_ptr<ir::Addr> &addr) {
  assert(!dyc<ir::Label>(addr));
  if (dyc<ir::GlobalReg>(addr))
    return Value(Value::Bottom);
  if (auto p = dyc<ir::IntLiteral>(addr))
    return Value(p->val);
  assert(dyc<ir::LocalReg>(addr));
  return values[ir::getLocalRegIdentifier(addr)];
}

} // namespace mocker

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

bool SparseSimpleConstantPropagation::operator()() {
  buildInstDefineAndInstsUse();
  initialize();
  propagate();
  rewrite();
  removeDeletedInsts(func);
  //    std::cerr << "SSCP: Modify " << modificationCnt << " insts in "
  //              << func.getIdentifier() << std::endl;
  return modificationCnt != 0;
}

void SparseSimpleConstantPropagation::buildInstDefineAndInstsUse() {
  auto updateInstsUse = [&](const std::shared_ptr<ir::IRInst> &inst) {
    auto operands = ir::getOperandsUsed(inst);
    for (auto &operand : operands) {
      if (auto p = ir::dycLocalReg(operand))
        instsUse[p->getIdentifier()].emplace_back(inst);
    }
  };

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto dest = ir::getDest(inst);
      if (!dest)
        continue;
      if (ir::dycGlobalReg(dest))
        continue;
      auto destName = dest->getIdentifier();
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
      const auto &destName = dest->getIdentifier();
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
    auto dest = ir::dycLocalReg(ir::getDest(inst));
    if (dest) {
      auto destName = dest->getIdentifier();
      auto val = values[destName];
      if (val.type == Value::Constant) {
        // All the insts that use this value will be rewrite so that they use
        // the constant directly. This inst can therefore be deleted.
        ++modificationCnt;
        return std::make_shared<ir::Deleted>();
      }
    }

    // Rewrite the operands
    auto operands = ir::getOperandsUsed(inst);
    for (auto &operand : operands) {
      auto reg = ir::dycLocalReg(operand);
      if (!reg)
        continue;
      auto name = reg->getIdentifier();
      auto val = values[name];
      if (val.type == Value::Constant) {
        ++modificationCnt;
        operand = std::make_shared<ir::IntLiteral>(val.val);
      }
    }
    return ir::copyWithReplacedOperands(inst, operands);
  };

  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts())
      inst = rewriteIfPossible(inst);
  }
}

SparseSimpleConstantPropagation::Value
SparseSimpleConstantPropagation::computeValue(const std::string &destName) {
  auto inst = instDefine.at(destName);
  if (auto p = ir::dyc<ir::Assign>(inst)) {
    return getValue(p->getOperand());
  }
  if (auto p = ir::dyc<ir::ArithUnaryInst>(inst)) {
    if (auto v = ir::dyc<ir::IntLiteral>(p->getOperand()))
      return Value(p->getOp() == ir::ArithUnaryInst::BitNot ? ~v->getVal()
                                                            : -v->getVal());
    if (!ir::dycLocalReg(p->getOperand()))
      return Value(Value::Bottom);
    auto operandV = values[ir::dyc<ir::Reg>(p->getOperand())->getIdentifier()];
    if (operandV.type == Value::Constant)
      return Value(p->getOp() == ir::ArithUnaryInst::BitNot ? ~operandV.val
                                                            : -operandV.val);
    return operandV;
  }
  if (auto p = ir::dyc<ir::ArithBinaryInst>(inst)) {
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
    auto lhsV = getValue(p->getLhs()), rhsV = getValue(p->getRhs());
    if ((p->getOp() == ir::ArithBinaryInst::Mul ||
         p->getOp() == ir::ArithBinaryInst::BitAnd) &&
        ((lhsV.type == Value::Constant && lhsV.val == 0) ||
         (rhsV.type == Value::Constant && rhsV.val == 0)))
      return Value(0);

    if (lhsV.type == Value::Bottom || rhsV.type == Value::Bottom)
      return Value(Value::Bottom);
    if (lhsV.type == Value::Constant && rhsV.type == Value::Constant) {
      if ((p->getOp() == ir::ArithBinaryInst::Div ||
           p->getOp() == ir::ArithBinaryInst::Mod) &&
          rhsV.val == 0)
        return Value(Value::Bottom);
      return Value(compute(p->getOp(), lhsV.val, rhsV.val));
    }
    return Value(Value::Top);
  }
  if (auto p = ir::dyc<ir::RelationInst>(inst)) {
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
    auto lhsV = getValue(p->getLhs()), rhsV = getValue(p->getRhs());
    if (lhsV.type == Value::Bottom || rhsV.type == Value::Bottom)
      return Value(Value::Bottom);
    if (lhsV.type == Value::Constant && rhsV.type == Value::Constant)
      return Value(compute(p->getOp(), lhsV.val, rhsV.val));
    return Value(Value::Top);
  }
  if (ir::dyc<ir::Load>(inst) || ir::dyc<ir::AllocVar>(inst) ||
      ir::dyc<ir::Malloc>(inst) || ir::dyc<ir::Call>(inst))
    return Value(Value::Bottom);
  if (auto p = ir::dyc<ir::Phi>(inst)) {
    Optional<std::int64_t> lastVal;
    for (auto &option : p->getOptions()) {
      if (auto reg = ir::dycLocalReg(option.first)) {
        if (reg->getIdentifier() == ".phi_nan")
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
    return Value((bool)lastVal ? lastVal.value() : 0);
  }
  // Store, StrCpy, Branch, Jump, Ret,
  assert(false);
}

SparseSimpleConstantPropagation::Value
SparseSimpleConstantPropagation::getValue(
    const std::shared_ptr<ir::Addr> &addr) {
  assert(!ir::dyc<ir::Label>(addr));
  if (ir::dycGlobalReg(addr))
    return Value(Value::Bottom);
  if (auto p = ir::dyc<ir::IntLiteral>(addr))
    return Value(p->getVal());
  assert(ir::dycLocalReg(addr));
  return values[ir::dyc<ir::Reg>(addr)->getIdentifier()];
}

} // namespace mocker

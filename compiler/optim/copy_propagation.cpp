#include "copy_propagation.h"

#include <cassert>
#include <functional>
#include <unordered_set>

#include "ir/helper.h"

namespace mocker {

CopyPropagation::CopyPropagation(ir::FunctionModule &func) : FuncPass(func) {}

bool CopyPropagation::operator()() {
  buildValue();
  rewrite();
  return cnt != 0;
}

void CopyPropagation::buildValue() {
  std::unordered_set<std::size_t> visited;

  std::function<void(std::size_t)> impl = [this, &visited,
                                           &impl](std::size_t bbLabel) {
    if (visited.find(bbLabel) != visited.end())
      return;
    visited.emplace(bbLabel);
    const auto &bb = func.getBasicBlock(bbLabel);

    for (auto &inst : bb.getInsts()) {
      auto assign = ir::dyc<ir::Assign>(inst);
      if (!assign)
        continue;
      auto destName = ir::dyc<ir::LocalReg>(assign->getDest())->getIdentifier();
      auto val = assign->getOperand();
      if (ir::dyc<ir::IntLiteral>(val) || ir::dyc<ir::GlobalReg>(val)) {
        value[destName] = val;
        continue;
      }
      if (auto reg = ir::dyc<ir::LocalReg>(val)) {
        auto iter = value.find(reg->getIdentifier());
        if (iter != value.end()) {
          value[destName] = iter->second;
          continue;
        }
        auto &tmp = value[destName];
        tmp = val;
        continue;
      }
      assert(false);
    }

    for (auto suc : bb.getSuccessors())
      impl(suc);
  };

  impl(func.getFirstBB()->getLabelID());
}

void CopyPropagation::rewrite() {
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      auto operands = ir::getOperandsUsed(inst);
      for (auto &operand : operands) {
        auto reg = ir::dyc<ir::LocalReg>(operand);
        if (!reg)
          continue;
        auto iter = value.find(reg->getIdentifier());
        if (iter == value.end())
          continue;
        operand = iter->second;
        ++cnt;
      }
      inst = ir::copyWithReplacedOperands(inst, operands);
    }
  }
}

} // namespace mocker

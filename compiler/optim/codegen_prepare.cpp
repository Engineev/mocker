#include "codegen_prepare.h"

#include <cassert>
#include <functional>
#include <unordered_set>
#include <vector>

#include "helper.h"
#include "ir/helper.h"
#include "set_operation.h"

namespace mocker {
namespace {

std::vector<std::size_t> getPreOrder(const ir::FunctionModule &func) {
  std::vector<std::size_t> res;

  std::unordered_set<std::size_t> visited;
  std::function<void(std::size_t)> dfs = [&res, &func, &visited,
                                          &dfs](std::size_t cur) {
    if (isIn(visited, cur))
      return;
    visited.emplace(cur);
    res.emplace_back(cur);
    for (auto suc : func.getBasicBlock(cur).getSuccessors())
      dfs(suc);
  };
  dfs(func.getFirstBBLabel());
  return res;
}

} // namespace

bool CodegenPreparation::operator()() {
  sortBlocks();
  func.buildContext();
  for (auto &bb : func.getMutableBBs())
    scheduleCmps(bb);
  removeDeletedInsts(func);
  return false;
}

void CodegenPreparation::sortBlocks() {
  loopTree.init(func);

  const auto PreOrder = getPreOrder(func);

  std::unordered_set<std::size_t> visited;
  std::vector<std::size_t> order;
  for (auto n : PreOrder) {
    if (isIn(visited, n) && order.back() != n) // can not be scheduled
      continue;

    if (!isIn(visited, n)) {
      order.emplace_back(n);
      visited.emplace(n);
    }

    auto br = ir::dyc<ir::Branch>(func.getBasicBlock(n).getInsts().back());
    if (!br) {
      continue;
    }

    if (loopTree.isLoopHeader(n)) {
      auto nxt = br->getThen()->getID();
      order.emplace_back(nxt);
      visited.emplace(nxt);
      continue;
    }

    auto &thenBB = func.getBasicBlock(br->getThen()->getID());
    auto &elseBB = func.getBasicBlock(br->getElse()->getID());
    if (ir::dyc<ir::Ret>(thenBB.getInsts().back()) &&
        !isIn(visited, elseBB.getLabelID())) {
      order.emplace_back(elseBB.getLabelID());
      visited.emplace(elseBB.getLabelID());
    } else if (!isIn(visited, thenBB.getLabelID())) {
      order.emplace_back(thenBB.getLabelID());
      visited.emplace(thenBB.getLabelID());
    }
  }

  assert(order.size() == PreOrder.size());

  ir::BasicBlockList newBBs;
  for (auto cur : order) {
    for (auto &bb : func.getBBs()) {
      if (bb.getLabelID() != cur)
        continue;
      newBBs.emplace_back(bb);
      break;
    }
  }
  func.getMutableBBs() = std::move(newBBs);
}

void CodegenPreparation::scheduleCmps(ir::BasicBlock &bb) {
  auto br = ir::dyc<ir::Branch>(bb.getInsts().back());
  if (!br)
    return;
  auto condition = ir::dycLocalReg(br->getCondition());
  if (!condition)
    return;

  auto riter = (++bb.getMutableInsts().rbegin());
  auto rend = bb.getMutableInsts().rend();
  for (; riter != rend; ++riter) {
    auto operands = ir::getOperandsUsed(*riter);
    for (auto &operand : operands) {
      auto reg = ir::dycLocalReg(operand);
      if (reg && reg->getIdentifier() == condition->getIdentifier())
        return;
    }

    auto def = ir::getDest(*riter);
    if (def && def->getIdentifier() == condition->getIdentifier())
      break;
  }
  if (riter == rend)
    return;

  bb.appendInstBeforeTerminator(*riter);
  *riter = std::make_shared<ir::Deleted>();
}

} // namespace mocker

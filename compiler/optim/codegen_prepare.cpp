#include "codegen_prepare.h"

#include <cassert>
#include <functional>
#include <unordered_set>
#include <vector>

#include "analysis/defuse.h"
#include "helper.h"
#include "ir/helper.h"
#include "ir/ir_inst.h"
#include "optim/global_value_numbering.h"
#include "set_operation.h"

#include "ir/printer.h"
#include <iostream>

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
  naiveStrengthReduction();
  removeDeletedInsts(func);
  for (auto &bb : func.getMutableBBs())
    removeRedundantLoadStore(bb);
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

  // find the instruction that define the condition reg
  auto riter = (++bb.getMutableInsts().rbegin());
  auto rend = bb.getMutableInsts().rend();
  for (; riter != rend; ++riter) {
    auto operands = ir::getOperandsUsed(*riter);
    for (auto &operand : operands) {
      auto reg = ir::dycLocalReg(operand);
      if (reg && reg->getIdentifier() == condition->getIdentifier())
        return;
    }

    auto defInst = *riter;
    if (ir::dyc<ir::Call>(defInst) || ir::dyc<ir::Load>(defInst))
      return;
    if (ir::dyc<ir::Phi>(defInst) &&
        !ir::dyc<ir::Phi>(*(++bb.getInsts().rbegin()))) {
      return;
    }

    auto def = ir::getDest(defInst);
    if (def && def->getIdentifier() == condition->getIdentifier())
      break;
  }
  if (riter == rend)
    return;

  //  std::cerr << ir::fmtInst(*riter) << std::endl;
  bb.appendInstBeforeTerminator(*riter);
  *riter = std::make_shared<ir::Deleted>();
}

namespace {

int getNonzeroPos(std::uint64_t n) {
  if (n & (n - 1))
    return -1;
  for (int i = 0; i < 8; ++i) {
    if (n & (1 << i))
      return i;
  }
  assert(false);
}

} // namespace

void CodegenPreparation::naiveStrengthReduction() {
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      auto binary = ir::dyc<ir::ArithBinaryInst>(inst);
      if (!binary)
        continue;
      auto lhs = binary->getLhs(), rhs = binary->getRhs();
      // Associativity is required here
      if (ir::dyc<ir::IntLiteral>(lhs) &&
          binary->getOp() != ir::ArithBinaryInst::Div)
        std::swap(lhs, rhs);
      auto lit = ir::dyc<ir::IntLiteral>(rhs);

      if (binary->getOp() == ir::ArithBinaryInst::Mul ||
          binary->getOp() == ir::ArithBinaryInst::Div) {
        if (!lit || lit->getVal() <= 0)
          continue;
        auto pos = getNonzeroPos(lit->getVal());
        if (pos == -1)
          continue;
        inst = std::make_shared<ir::ArithBinaryInst>(
            binary->getDest(),
            binary->getOp() == ir::ArithBinaryInst::Mul
                ? ir::ArithBinaryInst::Shl
                : ir::ArithBinaryInst::Shr,
            lhs, std::make_shared<ir::IntLiteral>(pos));
        continue;
      }

      if (binary->getOp() == ir::ArithBinaryInst::Add && lit &&
          lit->getVal() == 0) {
        inst = std::make_shared<ir::Assign>(binary->getDest(), lhs);
        continue;
      }
    }
  }
}

void CodegenPreparation::removeRedundantLoadStore(ir::BasicBlock &bb) {
  auto iter = bb.getMutableInsts().begin();
  auto nextIter = iter;
  nextIter++;

  for (auto end = bb.getMutableInsts().end(); nextIter != end;
       ++iter, ++nextIter) {
    auto load = ir::dyc<ir::Load>(*iter);
    auto store = ir::dyc<ir::Store>(*nextIter);
    if (!load || !store)
      continue;
    auto lAddr = ir::dyc<ir::Reg>(load->getAddr());
    auto sAddr = ir::dyc<ir::Reg>(store->getAddr());
    assert(lAddr && sAddr);
    if (lAddr->getIdentifier() != sAddr->getIdentifier())
      continue;
    *iter = std::make_shared<ir::Deleted>();
    *nextIter = std::make_shared<ir::Deleted>();
  }
}

} // namespace mocker

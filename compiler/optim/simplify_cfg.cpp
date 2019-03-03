#include "simplify_cfg.h"

#include <functional>
#include <iostream>
#include <queue>
#include <unordered_set>

#include "helper.h"

#include "ir/printer.h"

namespace mocker {

void deletePhiOptionInBB(ir::BasicBlock &bb, std::size_t toBeDeleted) {
  for (auto &inst : bb.getMutableInsts()) {
    auto phi = ir::dyc<ir::Phi>(inst);
    if (!phi)
      break;
    inst = deletePhiOption(phi, toBeDeleted);
  }
}

RewriteBranches::RewriteBranches(ir::FunctionModule &func) : FuncPass(func) {}

bool RewriteBranches::operator()() {
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      auto br = ir::dyc<ir::Branch>(inst);
      if (!br)
        continue;
      auto condition = ir::cdyc<ir::IntLiteral>(br->getCondition());
      if (!condition)
        continue;
      ++cnt;
      auto target = condition->val ? br->getThen() : br->getElse();
      inst = std::make_shared<ir::Jump>(ir::dyc<ir::Label>(copy(target)));
      auto notTarget = condition->val ? br->getElse() : br->getThen();
      deletePhiOptionInBB(func.getMutableBasicBlock(notTarget->id),
                          bb.getLabelID());
    }
  }

  return cnt != 0;
}

RemoveUnreachableBlocks::RemoveUnreachableBlocks(ir::FunctionModule &func)
    : FuncPass(func) {}

bool RemoveUnreachableBlocks::operator()() {
  std::unordered_set<std::size_t> reachable;
  std::function<void(std::size_t cur)> visit = [&visit, &reachable,
                                                this](std::size_t cur) {
    if (reachable.find(cur) != reachable.end())
      return;
    reachable.emplace(cur);
    for (auto suc : func.getBasicBlock(cur).getSuccessors())
      visit(suc);
  };
  visit(func.getFirstBB()->getLabelID());

  for (auto &bb : func.getBBs()) {
    if (reachable.find(bb.getLabelID()) != reachable.end())
      continue;
    for (auto &succ : bb.getSuccessors()) {
      deletePhiOptionInBB(func.getMutableBasicBlock(succ), bb.getLabelID());
    }
  }

  cnt = func.getBBs().size() - reachable.size();
  func.getMutableBBs().remove_if([&reachable](const ir::BasicBlock &bb) {
    return reachable.find(bb.getLabelID()) == reachable.end();
  });
  return cnt != 0;
}

MergeBlocks::MergeBlocks(ir::FunctionModule &func) : FuncPass(func) {}

bool MergeBlocks::operator()() {
  auto preds = buildBlockPredecessors(func);
  std::vector<std::size_t> mergeable;
  for (auto &kv : preds) {
    if (kv.second.size() != 1)
      continue;
    const auto &pred = func.getBasicBlock(kv.second.at(0));
    if (pred.getSuccessors().size() == 1)
      mergeable.emplace_back(kv.first);
  }

  auto toBeMerged = findFurthest(mergeable);
  auto isMergeable = [&preds, this](std::size_t u) -> bool {
    auto pred = preds[u];
    if (pred.size() != 1)
      return false;
    const auto &predBB = func.getBasicBlock(pred.at(0));
    return predBB.isCompleted() && predBB.getSuccessors().size() == 1;
  };
  auto merge = [this](std::size_t pred, std::size_t bbLabel) {
    auto &predBB = func.getMutableBasicBlock(pred);
    auto &bb = func.getMutableBasicBlock(bbLabel);
    for (auto succ : bb.getSuccessors()) {
      auto &succBB = func.getMutableBasicBlock(succ);
      for (auto &inst : succBB.getMutableInsts()) {
        auto phi = ir::dyc<ir::Phi>(inst);
        if (!phi)
          break;
        inst = replacePhiOption(phi, bbLabel, pred);
      }
    }
    predBB.getMutableInsts().pop_back();
    predBB.getMutableInsts().splice(predBB.getMutableInsts().end(),
                                    bb.getMutableInsts());
  };

  for (auto u : toBeMerged) {
    std::cerr.flush();
    while (isMergeable(u)) {
      auto pred = preds.at(u).at(0);
      merge(pred, u);
      u = pred;
    }
  }

  func.getMutableBBs().remove_if(
      [](const ir::BasicBlock &bb) { return bb.getInsts().empty(); });

  //  std::cerr << "MergeBlocks: Merged " << mergeable.size() << " BBs in "
  //            << func.getIdentifier() << std::endl;
  return mergeable.size() != 0;
}

std::vector<std::size_t>
MergeBlocks::findFurthest(const std::vector<std::size_t> &mergeable) const {
  std::vector<std::size_t> res;
  std::unordered_set<std::size_t> isMergeable(mergeable.begin(),
                                              mergeable.end());

  for (auto bb : mergeable) {
    auto succ = func.getBasicBlock(bb).getSuccessors();
    if (succ.size() == 1 && isMergeable.find(succ.at(0)) != isMergeable.end())
      continue;
    res.emplace_back(bb);
  }

  return res;
}

} // namespace mocker
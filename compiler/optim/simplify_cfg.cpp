#include "simplify_cfg.h"

#include <functional>
#include <iostream>
#include <queue>
#include <unordered_set>

#include "helper.h"

#include "ir/printer.h"

// topoSort
namespace mocker {
namespace {

std::vector<std::size_t> topoSort(const std::vector<std::size_t> &nodes,
                                  const ir::FunctionModule &func) {
  // label -> predcessors
  std::unordered_map<std::size_t, std::unordered_set<std::size_t>> subGraph;
  {
    std::unordered_set<std::size_t> nodesUsed{nodes.begin(), nodes.end()};
    auto predGraph = buildBlockPredecessors(func);
    for (auto node : nodesUsed) {
      auto &preds = subGraph[node];
      for (auto pred : predGraph.at(node))
        if (nodesUsed.find(pred) != nodesUsed.end())
          preds.emplace(pred);
    }
  }

  //  std::cerr << "\nunsorted: ";
  //  for (auto node : nodes)
  //    std::cerr << node << ", ";
  //  std::cerr << std::endl;

  std::vector<std::size_t> res;
  while (!subGraph.empty()) {
    //    std::cerr << "subgraph:\n";
    //    for (auto &kv : subGraph) {
    //      std::cerr << kv.first << " <- ";
    //      for (auto &node : kv.second)
    //        std::cerr << node << ", ";
    //      std::cerr << "\n";
    //    }

    std::vector<std::size_t> nodesWithNoPred;
    for (auto &kv : subGraph)
      if (kv.second.empty())
        nodesWithNoPred.emplace_back(kv.first);

    for (auto node : nodesWithNoPred) {
      res.emplace_back(node);
      subGraph.erase(subGraph.find(node));
      for (auto succ : func.getBasicBlock(node).getSuccessors()) {
        auto iter = subGraph.find(succ);
        if (iter == subGraph.end())
          continue;
        iter->second.erase(iter->second.find(node));
      }
    }

    if (nodesWithNoPred.empty()) // cyclic
      break;
  }

  //  std::cerr << "sorted: ";
  //  for (auto node : res)
  //    std::cerr << node << ", ";
  //  std::cerr << "\n";

  return res;
}

} // namespace
} // namespace mocker

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

  mergeable = topoSort(mergeable, func);
  std::reverse(mergeable.begin(), mergeable.end());

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

  for (auto u : mergeable) {
    merge(preds.at(u).at(0), u);
  }

  func.getMutableBBs().remove_if(
      [](const ir::BasicBlock &bb) { return bb.getInsts().empty(); });

  //  std::cerr << "MergeBlocks: Merged " << mergeable.size() << " BBs in "
  //            << func.getIdentifier() << std::endl;
  return !mergeable.empty();
}

RemoveTrivialBlocks::RemoveTrivialBlocks(ir::FunctionModule &func)
    : FuncPass(func) {}

bool RemoveTrivialBlocks::operator()() {
  std::vector<std::size_t> toBeRemoved;
  {
    for (auto &bb : func.getMutableBBs()) {
      if (func.getFirstBB()->getLabelID() == bb.getLabelID())
        continue;
      auto jump = ir::dyc<ir::Jump>(bb.getInsts().front());
      if (!jump)
        continue;
      bool ok = true;
      for (auto &inst : func.getBasicBlock(jump->getLabel()->id).getInsts()) {
        auto phi = ir::dyc<ir::Phi>(inst);
        if (!phi)
          break;
        for (auto &option : phi->getOptions()) {
          if (option.second->id == bb.getLabelID()) {
            ok = false;
            break;
          }
        }
        if (!ok)
          break;
      }
      if (!ok)
        continue;
      toBeRemoved.emplace_back(bb.getLabelID());
    }
  }
  toBeRemoved = topoSort(toBeRemoved, func);
  std::reverse(toBeRemoved.begin(), toBeRemoved.end());

  auto predMap = buildBlockPredecessors(func);

  for (auto &bbLabel : toBeRemoved) {
    auto &bb = func.getMutableBasicBlock(bbLabel);
    auto jump = ir::dyc<ir::Jump>(bb.getInsts().front());
    assert(jump);
    auto targetLabel = jump->getLabel()->id;
    auto curLabel = bb.getLabelID();
    const auto &preds = predMap.at(curLabel);
    assert(!preds.empty());

    for (auto predLabel : preds) {
      auto &pred = func.getMutableBasicBlock(predLabel);
      auto &terminator = pred.getMutableInsts().back();
      terminator = replaceTerminatorLabel(terminator, curLabel, targetLabel);
    }

    auto newPhi = [&preds, curLabel](const std::shared_ptr<ir::Phi> &old) {
      auto dest = ir::copy(old->getDest());
      auto oldOptions = old->getOptions();
      std::vector<ir::Phi::Option> options;
      std::shared_ptr<ir::Addr> val;
      for (auto &option : oldOptions) {
        if (option.second->id != curLabel) {
          options.emplace_back(
              std::make_pair(ir::copy(option.first),
                             ir::dyc<ir::Label>(ir::copy(option.second))));
          continue;
        }
        val = ir::copy(option.first);
      }
      assert(val);
      for (auto pred : preds)
        options.emplace_back(
            std::make_pair(val, std::make_shared<ir::Label>(pred)));
      return std::make_shared<ir::Phi>(std::move(dest), std::move(options));
    };

    auto &succ = func.getMutableBasicBlock(targetLabel);
    for (auto &inst : succ.getMutableInsts()) {
      auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        break;
      inst = newPhi(phi);
    }
  }

  std::unordered_set<std::size_t> removable{toBeRemoved.begin(),
                                            toBeRemoved.end()};
  func.getMutableBBs().remove_if([&removable](const ir::BasicBlock &bb) {
    return removable.find(bb.getLabelID()) != removable.end();
  });

  //  std::cerr << "RemoveTrivialBlocks: removed " << toBeRemoved.size()
  //            << " blocks\n";
  return !toBeRemoved.empty();
}

} // namespace mocker
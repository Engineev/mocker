#include "dominance.h"

#include <cassert>
#include <functional>

#include "optim/helper.h"
#include "set_operation.h"

namespace mocker {
namespace {
template <class T> using LabelMap = std::unordered_map<std::size_t, T>;
using LabelSet = std::unordered_set<std::size_t>;

std::unordered_set<std::size_t> buildDominatingImpl(
    const ir::FunctionModule &func, std::size_t node,
    const std::unordered_map<std::size_t, std::vector<std::size_t>> &preds) {
  // Construct the set [avoidable] of nodes which are reachable from the entry
  // without passing [node]. The nodes dominated by [node] are just the nodes
  // which are not in [avoidable].
  bool reverse = !preds.empty();
  std::unordered_set<std::size_t> avoidable;
  std::function<void(std::size_t cur)> visit =
      [&visit, &avoidable, node, &preds, &func, reverse](std::size_t cur) {
        if (cur == node || avoidable.find(cur) != avoidable.end())
          return;
        avoidable.emplace(cur);
        if (!reverse) {
          for (auto suc : func.getBasicBlock(cur).getSuccessors())
            visit(suc);
        } else {
          for (auto pred : preds.at(cur))
            visit(pred);
        }
      };
  if (!reverse)
    visit(func.getFirstBBLabel());
  else {
    for (auto &bb : func.getBBs())
      if (ir::dyc<ir::Ret>(bb.getInsts().back()))
        visit(bb.getLabelID());
  }

  std::unordered_set<std::size_t> res;
  for (const auto &bb : func.getBBs()) {
    if (!isIn(avoidable, bb.getLabelID()))
      res.emplace(bb.getLabelID());
  }
  return res;
}

} // namespace

void DominatorTree::init(const ir::FunctionModule &func, bool reverse_) {
  reverse = reverse_;
  for (auto &bb : func.getBBs()) {
    auto label = bb.getLabelID();
    dominating[label] = dominators[label] = {};
    immediateDominator[label] = (std::size_t)-1;
    dominatorTree[label] = dominanceFrontier[label] = {};
  }
  buildDominating(func);
  buildDominators();
  buildImmediateDominator(func);
  buildDominatorTree();
  buildDominanceFrontier(func);
}

void DominatorTree::buildDominating(const ir::FunctionModule &func) {
  for (const auto &bb : func.getBBs()) {
    auto preds =
        reverse ? buildBlockPredecessors(func)
                : std::unordered_map<std::size_t, std::vector<std::size_t>>();
    dominating[bb.getLabelID()] =
        buildDominatingImpl(func, bb.getLabelID(), preds);
  }
}

bool DominatorTree::isDominating(std::size_t u, std::size_t v) const {
  return isIn(dominating.at(u), v);
}

bool DominatorTree::isStrictlyDominating(std::size_t u, std::size_t v) const {
  return u != v && isDominating(u, v);
}

void DominatorTree::buildDominators() {
  for (auto &kv : dominating) {
    auto pnt = kv.first;
    for (auto child : kv.second)
      dominators[child].emplace(pnt);
  }
}

void DominatorTree::buildImmediateDominator(const ir::FunctionModule &func) {
  // For a faster algorithm, see [Lengauer, Tarjan, 1979]

  for (const auto &bb : func.getBBs()) {
    auto node = bb.getLabelID();
    if (!reverse && node == func.getFirstBBLabel()) {
      immediateDominator[node] = std::size_t(-1);
      continue;
    }
    if (reverse && bb.getSuccessors().empty()) {
      immediateDominator[node] = std::size_t(-1);
      continue;
    }

    const auto &Dominators = dominators.at(node);
    for (auto dominator : Dominators) {
      if (dominator == node) // is not strict
        continue;
      bool flag = true;
      for (auto otherDominator : Dominators) {
        if (otherDominator == node)
          continue;
        if (isStrictlyDominating(dominator, otherDominator)) {
          flag = false;
          break;
        }
      }
      if (flag) {
        immediateDominator[node] = dominator;
        break;
      }
    }
  }
}

void DominatorTree::buildDominatorTree() {
  for (auto &kv : immediateDominator) {
    if (kv.second == (std::size_t)-1)
      continue;
    dominatorTree[kv.second].emplace(kv.first);
  }
}

void DominatorTree::buildDominanceFrontier(const ir::FunctionModule &func) {
  auto collectCFGEdge = [&func, this] {
    std::vector<std::pair<std::size_t, std::size_t>> res;
    for (const auto &bb : func.getBBs()) {
      auto succ = bb.getSuccessors();
      for (auto to : succ) {
        if (reverse)
          res.emplace_back(to, bb.getLabelID());
        else
          res.emplace_back(bb.getLabelID(), to);
      }
    }
    return res;
  };

  auto edges = collectCFGEdge();
  for (const auto &edge : edges) {
    auto a = edge.first, b = edge.second;
    auto x = a;
    while (x != std::size_t(-1) && !isStrictlyDominating(x, b)) {
      dominanceFrontier[x].emplace(b);
      x = immediateDominator[x];
    }
  }
}

} // namespace mocker

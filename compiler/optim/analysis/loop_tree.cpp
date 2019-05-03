#include "loop_tree.h"

#include <cassert>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "dominance.h"
#include "optim/helper.h"
#include "set_operation.h"

#include <iostream>

namespace mocker {
namespace {

std::vector<std::pair<std::size_t, std::size_t>>
findBackEdges(const DominatorTree &dominatorTree,
              const ir::FunctionModule &func) {
  std::vector<std::pair<std::size_t, std::size_t>> res;

  auto isBackEdge = [&dominatorTree](std::size_t from, std::size_t to) {
    return dominatorTree.isDominating(to, from);
  };

  for (const auto &bb : func.getBBs()) {
    auto succ = bb.getSuccessors();
    auto from = bb.getLabelID();
    for (auto to : succ) {
      if (isBackEdge(from, to))
        res.emplace_back(from, to);
    }
  }

  return res;
}

// Find the natural loop corresponding to the back edge n -> h
std::vector<std::size_t> findNaturalLoop(const DominatorTree &dominatorTree,
                                         const ir::FunctionModule &func,
                                         std::size_t n, std::size_t h) {
  // NaturalLoop(h, n) =
  //   {x | h dominates x and there exists a path from x to n not passing h}
  // To construct NaturalLoop(h, n), we first traverse the CFG backward from n
  // and find all nodes that can be reached without passing h. Then, we filter
  // out the ones that is not dominated by h.

  auto Preds = buildBlockPredecessors(func);

  std::unordered_set<std::size_t> visited;
  std::function<void(std::size_t)> dfs = [h, &Preds, &dfs,
                                          &visited](std::size_t cur) {
    if (isIn(visited, cur) || cur == h)
      return;
    visited.emplace(cur);
    for (auto pred : Preds.at(cur))
      dfs(pred);
  };
  dfs(n);

  std::vector<std::size_t> res;
  for (auto x : visited) {
    if (dominatorTree.isDominating(h, x))
      res.emplace_back(x);
  }
  return res;
}

std::unordered_map<std::size_t, std::unordered_set<std::size_t>> buildLoopTree(
    std::unordered_map<std::size_t, std::unordered_set<std::size_t>> loops) {
  std::unordered_map<std::size_t, std::unordered_set<std::size_t>> res;

  //  std::cerr << "Loops:\n";
  //  for (auto & loop : loops) {
  //    if (loop.second.size() == 1)
  //      continue;
  //    std::cerr << loop.first << ": ";
  //    for (auto & n : loop.second)
  //      std::cerr << n << ", ";
  //    std::cerr << std::endl;
  //  }

  const auto Bak = loops;

  loops.clear();
  std::vector<std::size_t> loopNodes;
  for (auto &kv : Bak) {
    res[kv.first] = {};
    if (kv.second.size() > 1) {
      loopNodes.emplace_back(kv.first);
      loops[kv.first] = Bak.at(kv.first);
    }
  }

  std::queue<std::size_t> worklist;
  std::vector<std::size_t> toBeRemoved;
  for (auto &kv : loops) {
    intersectSet(kv.second, loopNodes);
    assert(!kv.second.empty());
    if (kv.second.size() == 1) {
      worklist.emplace(kv.first);
      toBeRemoved.emplace_back(kv.first);
    }
  }
  for (auto &n : toBeRemoved) {
    loops.erase(loops.find(n));
  }
  toBeRemoved.clear();

  // build
  while (!worklist.empty()) {
    auto leaf = worklist.front();
    worklist.pop();

    toBeRemoved.clear();
    for (auto &kv : loops) {
      auto pnt = kv.first;
      auto &loop = kv.second;
      if (!isIn(loop, leaf))
        continue;
      subSet(loop, Bak.at(leaf));
      if (loop.size() == 1) {
        res[pnt].emplace(leaf);
        worklist.emplace(pnt);
        toBeRemoved.emplace_back(pnt);
      }
    }
    for (auto &n : toBeRemoved) {
      loops.erase(loops.find(n));
    }
  }

  //  std::cerr << "Loop tree:\n";
  //  for (auto & kv : res) {
  //    if (kv.second.empty())
  //      continue;
  //    std::cerr << kv.first << ": ";
  //    for (auto & n : kv.second)
  //      std::cerr << n << ", ";
  //    std::cerr << std::endl;
  //  }

  return res;
}

} // namespace

void LoopTree::init(const ir::FunctionModule &func) {
  root = func.getFirstBBLabel();
  auto dominatorTree = DominatorTree();
  dominatorTree.init(func);

  // init
  for (auto &bb : func.getBBs()) {
    loops[bb.getLabelID()] = {bb.getLabelID()};
  }
  // Add a pseudo-loop
  for (auto &bb : func.getBBs()) {
    loops[func.getFirstBBLabel()].emplace(bb.getLabelID());
  }

  const auto BackEdges = findBackEdges(dominatorTree, func);
  for (auto &backEdge : BackEdges) {
    auto h = backEdge.second, n = backEdge.first;
    const auto NaturalLoop = findNaturalLoop(dominatorTree, func, n, h);
    unionSet(loops.at(h), NaturalLoop);
  }

  loopTree = buildLoopTree(loops);
  buildDepth();
}

std::vector<std::size_t> LoopTree::postOrder() const {
  std::vector<std::size_t> res;

  std::unordered_set<std::size_t> visited;
  std::function<void(std::size_t)> dfs = [&res, &visited, &dfs,
                                          this](std::size_t cur) {
    if (isIn(visited, cur))
      return;
    visited.emplace(cur);
    auto children = loopTree.at(cur);
    for (auto ch : children)
      dfs(ch);
    res.emplace_back(cur);
  };
  dfs(root);

  return res;
}

void LoopTree::buildDepth() {
  std::function<void(std::size_t, std::size_t)> dfs =
      [this, &dfs](std::size_t cur, std::size_t curDepth) {
        for (auto &node : loops.at(cur))
          depth[node] = std::max(depth[node], curDepth);
        for (auto child : loopTree.at(cur))
          dfs(child, curDepth + 1);
      };
  dfs(root, 0);
}

} // namespace mocker

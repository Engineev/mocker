#ifndef MOCKER_DOMINANCE_H
#define MOCKER_DOMINANCE_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/module.h"

namespace mocker {

class DominatorTree {
public:
  void init(const ir::FunctionModule &func);

  // whether [u] dominates [v] ?
  bool isDominating(std::size_t u, std::size_t v) const;

  // whether [u] strictly dominates [v]
  bool isStrictlyDominating(std::size_t u, std::size_t v) const;

  const ir::LabelSet &getDominanceFrontier(std::size_t n) const {
    return dominanceFrontier.at(n);
  }

  const ir::LabelSet &getChildren(std::size_t n) const {
    return dominatorTree.at(n);
  }

private:
  void buildDominating(const ir::FunctionModule &func);

  void buildDominators();

  // Block n1 is said to be the immediate dominator of block n2 if
  // (1) n1 strictly dominates n2
  // (2) n1 does not strictly dominates any block that strictly dominates n2
  void buildImmediateDominator(const ir::FunctionModule &func);

  void buildDominatorTree();

  void buildDominanceFrontier(const ir::FunctionModule &func);

private:
  ir::LabelMap<ir::LabelSet> dominating;
  ir::LabelMap<ir::LabelSet> dominators;
  ir::LabelMap<std::size_t> immediateDominator;
  ir::LabelMap<ir::LabelSet> dominatorTree;
  ir::LabelMap<ir::LabelSet> dominanceFrontier;
};

} // namespace mocker

#endif // MOCKER_DOMINANCE_H

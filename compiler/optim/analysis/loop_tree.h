#ifndef MOCKER_LOOP_TREE_H
#define MOCKER_LOOP_TREE_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/module.h"

namespace mocker {

// Ch18, the tiger book.
class LoopTree {
public:
  void init(const ir::FunctionModule &func);

  const std::unordered_map<std::size_t, std::unordered_set<std::size_t>> &
  getLoops() const {
    return loops;
  }

  // inner loops first traversal
  std::vector<std::size_t> postOrder() const;

  bool isLoopHeader(std::size_t n) const {
    return loops.at(n).size() != 1;
  }

  std::size_t getDepth(std::size_t n) const { return depth.at(n); }

private:
  void buildDepth();

private:
  // The mapping from the headers to the union of the natural loops
  // If loops[x] is contains only itself, then x is not a header of any loop.
  std::unordered_map<std::size_t, std::unordered_set<std::size_t>> loops;

  // The loop tree, where each loop is represented by its header.
  std::unordered_map<std::size_t, std::unordered_set<std::size_t>> loopTree;
  std::size_t root = (std::size_t)-1;

  std::unordered_map<std::size_t, std::size_t> depth;
};

} // namespace mocker

#endif // MOCKER_LOOP_TREE_H

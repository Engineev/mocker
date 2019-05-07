#ifndef MOCKER_LOOP_INFO_H
#define MOCKER_LOOP_INFO_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "defuse.h"
#include "func_attr.h"
#include "ir/module.h"

namespace mocker {

// Ch18, the tiger book.
class LoopInfo {
public:
  void init(const ir::FunctionModule &func);

  // Also find all loop invariant variables
  void init(const ir::FunctionModule &func, const FuncAttr &funcAttr);

  const std::unordered_map<std::size_t, std::unordered_set<std::size_t>> &
  getLoops() const {
    return loops;
  }

  // inner loops first traversal
  std::vector<std::size_t> postOrder() const;

  bool isLoopHeader(std::size_t n) const { return loops.at(n).size() != 1; }

  std::size_t getDepth(std::size_t n) const { return depth.at(n); }

  const ir::RegSet getLoopInvariantVariables(std::size_t n) const {
    return loopInvariant.at(n);
  }

private:
  void buildDepth();

  void buildLoopInvariantVariables(const ir::FunctionModule &func,
                                   const FuncAttr &funcAttr);

  ir::RegSet findLoopInvariantVariables(const ir::FunctionModule &func,
                                        std::size_t header,
                                        const UseDefChain &useDef,
                                        const FuncAttr &funcAttr);

private:
  // The mapping from the headers to the union of the natural loops
  // If loops[x] is contains only itself, then x is not a header of any loop.
  std::unordered_map<std::size_t, std::unordered_set<std::size_t>> loops;

  // The loop tree, where each loop is represented by its header.
  std::unordered_map<std::size_t, std::unordered_set<std::size_t>> loopTree;
  std::size_t root = (std::size_t)-1;

  std::unordered_map<std::size_t, std::size_t> depth;

  std::unordered_map<std::size_t, ir::RegSet> loopInvariant;
};

} // namespace mocker

#endif // MOCKER_LOOP_INFO_H

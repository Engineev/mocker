// Move loop-invariant computations out of the loops.
// Note that only the time-consuming ones are moved.

#ifndef MOCKER_LOOPINV_H
#define MOCKER_LOOPINV_H

#include <unordered_map>
#include <unordered_set>

#include "analysis/defuse.h"
#include "analysis/func_attr.h"
#include "opt_pass.h"
#include "optim/analysis/loop_info.h"

namespace mocker {

class LoopInvariantCodeMotion : public FuncPass {
public:
  LoopInvariantCodeMotion(ir::FunctionModule &func, const FuncAttr &funcAttr)
      : FuncPass(func), funcAttr(funcAttr) {}

  bool operator()() override;

private:
  void insertPreHeaders();

  std::size_t processLoop(std::size_t header);

  std::unordered_set<ir::InstID>
  findLoopInvariantComputation(std::size_t header, const UseDefChain &useDef);

  void hoist(const std::unordered_set<std::size_t> &loopNodes,
             const std::unordered_set<ir::InstID> &invariant,
             std::size_t preHeader);

private:
  const FuncAttr &funcAttr;
  LoopInfo loopTree;
  std::unordered_map<std::size_t, std::size_t> preHeaders;
};

} // namespace mocker

#endif // MOCKER_LOOPINV_H

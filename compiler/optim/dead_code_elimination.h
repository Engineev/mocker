#ifndef MOCKER_DEAD_CODE_ELIMINATION_H
#define MOCKER_DEAD_CODE_ELIMINATION_H

#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "analysis/dominance.h"
#include "ir/ir_inst.h"
#include "opt_pass.h"

namespace mocker {

class DeadCodeElimination : public FuncPass {
public:
  explicit DeadCodeElimination(ir::FunctionModule &func);

  bool operator()() override;

private:
  bool isParameter(const std::string &identifier);

  void init();

  void mark();

  void sweep();

private:
  DominatorTree rdt;
  std::queue<std::shared_ptr<ir::IRInst>> worklist;
  std::unordered_map<ir::InstID, std::size_t> residingBB;
  std::unordered_set<ir::InstID> useful;
  std::unordered_set<std::size_t> usefulBB;  // contains a useful instruction
  std::size_t cnt = 0;
};

} // namespace mocker

#endif // MOCKER_DEAD_CODE_ELIMINATION_H

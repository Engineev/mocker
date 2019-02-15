#include "remove_dead_blocks.h"

#include <functional>
#include <unordered_set>

namespace mocker {

RemoveDeadBlocks::RemoveDeadBlocks(ir::FunctionModule &func) : FuncPass(func) {}

void RemoveDeadBlocks::operator()() {
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
  func.getMutableBBs().remove_if([&reachable](const ir::BasicBlock &bb) {
    return reachable.find(bb.getLabelID()) == reachable.end();
  });
}

} // namespace mocker

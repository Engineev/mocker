#ifndef MOCKER_REMOVE_DEAD_BLOCKS_H
#define MOCKER_REMOVE_DEAD_BLOCKS_H

#include "opt_pass.h"

namespace mocker {

// Remove the basic blocks that can not be reached from the entry.
class RemoveDeadBlocks : public FuncPass {
public:
  explicit RemoveDeadBlocks(ir::FunctionModule &func);

  const std::string &passID() const override {
    static const std::string id = "RemoveDeadBlocks";
    return id;
  }

  const std::vector<std::string> &prerequisites() const override {
    static const std::vector<std::string> res; // empty
    return res;
  }

  void operator()() override;
};

} // namespace mocker

#endif // MOCKER_REMOVE_DEAD_BLOCKS_H

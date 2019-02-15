#ifndef MOCKER_REMOVE_DEAD_BLOCKS_H
#define MOCKER_REMOVE_DEAD_BLOCKS_H

#include "opt_pass.h"

namespace mocker {

// Remove the basic blocks that can not be reached from the entry.
class RemoveDeadBlocks : public FuncPass {
public:
  static const std::string &PassID() {
    static const std::string id = "RemoveDeadBlocks";
    return id;
  }

  static const std::vector<std::string> &Prerequisites() {
    static const std::vector<std::string> res; // empty
    return res;
  }

public:
  explicit RemoveDeadBlocks(ir::FunctionModule &func);

  void operator()() override;
};

} // namespace mocker

#endif // MOCKER_REMOVE_DEAD_BLOCKS_H

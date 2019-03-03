#ifndef MOCKER_SIMPLIFY_CFG_H
#define MOCKER_SIMPLIFY_CFG_H

#include "opt_pass.h"

namespace mocker {

// If a Branch inst always take one branch, then rewrite it to a Jump inst
// and modify the phi-functions in its successors correspondingly.
class RewriteBranches : public FuncPass {
public:
  explicit RewriteBranches(ir::FunctionModule &func);

  bool operator()() override;

private:
  std::size_t cnt = 0;
};

// Remove a basic block if it is not connected to the entry.
class RemoveUnreachableBlocks : public FuncPass {
public:
  explicit RemoveUnreachableBlocks(ir::FunctionModule &func);

  bool operator()() override;

private:
  std::size_t cnt = 0;
};

// Merge a basic block into its predecessor if it has only one predecessor and
// it is the only successor of its predecessor.
class MergeBlocks : public FuncPass {
public:
  explicit MergeBlocks(ir::FunctionModule &func);

  bool operator()() override;

private:
  std::vector<std::size_t>
  findFurthest(const std::vector<std::size_t> &mergeable) const;
};

} // namespace mocker

#endif // MOCKER_SIMPLIFY_CFG_H

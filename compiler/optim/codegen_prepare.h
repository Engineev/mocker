#ifndef MOCKER_CODEGEN_PREPARE_H
#define MOCKER_CODEGEN_PREPARE_H

#include "opt_pass.h"

#include "optim/analysis/loop_info.h"

namespace mocker {

class CodegenPreparation : public FuncPass {
public:
  explicit CodegenPreparation(ir::FunctionModule &func) : FuncPass(func) {}

  bool operator()() override;

private:
  void sortBlocks();

  void scheduleCmps(ir::BasicBlock &bb);

  void naiveStrengthReduction();

  // remove sequences like:
  // x = load addr
  // store addr x
  void removeRedundantLoadStore(ir::BasicBlock & bb);

private:
  LoopInfo loopTree;
};

} // namespace mocker

#endif // MOCKER_CODEGEN_PREPARE_H

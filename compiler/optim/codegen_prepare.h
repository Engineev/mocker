#ifndef MOCKER_CODEGEN_PREPARE_H
#define MOCKER_CODEGEN_PREPARE_H

#include "opt_pass.h"

#include "analysis/loop_tree.h"

namespace mocker {

class CodegenPreparation : public FuncPass {
public:
  explicit CodegenPreparation(ir::FunctionModule &func) : FuncPass(func) {}

  bool operator()() override;

private:
  void sortBlocks();

  void scheduleCmps(ir::BasicBlock &bb);

  void naiveStrengthReduction();

private:
  LoopTree loopTree;
};

} // namespace mocker

#endif // MOCKER_CODEGEN_PREPARE_H

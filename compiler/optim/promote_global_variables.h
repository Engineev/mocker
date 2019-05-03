#ifndef MOCKER_PROMOTE_GLOBAL_VARIABLES_H
#define MOCKER_PROMOTE_GLOBAL_VARIABLES_H

#include "opt_pass.h"

#include "optim/analysis/func_attr.h"

namespace mocker {

// Promote global variables to stack so that the SSA pass can take them into
// consideration.
class PromoteGlobalVariables : public ModulePass {
public:
  explicit PromoteGlobalVariables(ir::Module &module);

  bool operator()() override;

private:
  void promoteGlobalVariables(ir::FunctionModule &func);

  FuncAttr funcGlobalVar;
};

} // namespace mocker

#endif // MOCKER_PROMOTE_GLOBAL_VARIABLES_H

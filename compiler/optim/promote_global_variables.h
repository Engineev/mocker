#ifndef MOCKER_PROMOTE_GLOBAL_VARIABLES_H
#define MOCKER_PROMOTE_GLOBAL_VARIABLES_H

#include "opt_pass.h"

#include <unordered_map>
#include <unordered_set>

namespace mocker {

// Promote global variables to stack so that the SSA pass can consider them.
class PromoteGlobalVariables : public ModulePass {
public:
  explicit PromoteGlobalVariables(ir::Module &module);

  void operator()() override;

private:
  void promoteGlobalVariables(ir::FunctionModule &func);

private:
  std::unordered_map<std::string, std::unordered_set<std::string>>
      globalVarUsed;
};

} // namespace mocker

#endif // MOCKER_PROMOTE_GLOBAL_VARIABLES_H

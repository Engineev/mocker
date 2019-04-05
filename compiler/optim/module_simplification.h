#ifndef MOCKER_MODULE_SIMPLIFICATION_H
#define MOCKER_MODULE_SIMPLIFICATION_H

#include "opt_pass.h"

namespace mocker {

class UnusedFunctionRemoval : public ModulePass {
public:
  explicit UnusedFunctionRemoval(ir::Module &module) : ModulePass(module) {}

  bool operator()() override;
};

} // namespace mocker

#endif // MOCKER_MODULE_SIMPLIFICATION_H

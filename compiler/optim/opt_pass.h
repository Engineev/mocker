#ifndef MOCKER_OPT_PASS_H
#define MOCKER_OPT_PASS_H

#include "ir/module.h"

namespace mocker {

class OptPass {
public:
  virtual ~OptPass() = default;

  virtual bool operator()() = 0;
};

class FuncPass : public OptPass {
public:
  explicit FuncPass(ir::FunctionModule &func) : func(func) {}

protected:
  ir::FunctionModule &func;
};

class BasicBlockPass : public OptPass {
public:
  explicit BasicBlockPass(ir::BasicBlock &bb) : bb(bb) {}

protected:
  ir::BasicBlock &bb;
};

class ModulePass : public OptPass {
public:
  explicit ModulePass(ir::Module &module) : module(module) {}

protected:
  ir::Module &module;
};

} // namespace mocker

#endif // MOCKER_OPT_PASS_H

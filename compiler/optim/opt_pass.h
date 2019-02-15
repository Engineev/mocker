#ifndef MOCKER_OPT_PASS_H
#define MOCKER_OPT_PASS_H

#include "ir/module.h"

namespace mocker {

class OptPass {
public:
  virtual ~OptPass() = default;
};

class FuncPass : public OptPass {
public:
  explicit FuncPass(ir::FunctionModule &func) : func(func) {}

  virtual void operator()() = 0;

protected:
  ir::FunctionModule &func;
};

class BasicBlockPass : public OptPass {
public:
  explicit BasicBlockPass(ir::BasicBlock &bb) : bb(bb) {}

  virtual void operator()() = 0;

protected:
  ir::BasicBlock &bb;
};

} // namespace mocker

#endif // MOCKER_OPT_PASS_H

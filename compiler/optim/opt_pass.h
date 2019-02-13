#ifndef MOCKER_OPT_PASS_H
#define MOCKER_OPT_PASS_H

#include "ir/module.h"

namespace mocker {

class OptPass {
public:
  OptPass() = default;

  virtual ~OptPass() = default;

  virtual const std::string &passID() const = 0;

  virtual const std::vector<std::string> &prerequisites() const = 0;
};

class FuncPass : public OptPass {
public:
  explicit FuncPass(ir::FunctionModule &func) : func(func) {}

  virtual void operator()() = 0;

protected:
  ir::FunctionModule &func;
};

} // namespace mocker

#endif // MOCKER_OPT_PASS_H

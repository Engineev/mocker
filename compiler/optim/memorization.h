// This pass is evil

#ifndef MOCKER_MEMORIZATION_H
#define MOCKER_MEMORIZATION_H

#include "opt_pass.h"

#include "analysis/func_attr.h"

namespace mocker {

class Memorization : public FuncPass {
public:
  Memorization(ir::FunctionModule &func, const FuncAttr &funcAttr,
               ir::Module &module)
      : FuncPass(func), funcAttr(funcAttr), module(module) {}

  bool operator()() override;

private:
  // check whether this function is suitable for memorization
  bool check() const;

  void rewrite();

private:
  const FuncAttr &funcAttr;
  ir::Module &module;
};

} // namespace mocker

#endif // MOCKER_MEMORIZATION_H

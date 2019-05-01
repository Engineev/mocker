#ifndef MOCKER_GLOBAL_CONST_INLINE_H
#define MOCKER_GLOBAL_CONST_INLINE_H

#include "opt_pass.h"
#include "ir/ir_inst.h"

namespace mocker {

class GlobalConstantInline : public ModulePass {
public:
  explicit GlobalConstantInline(ir::Module & module) : ModulePass(module) {}

  bool operator()() override;

private:
  // Find all global variables that are only defined once
  void checkFunc(const ir::FunctionModule &func);

  bool rewrite(ir::FunctionModule & func);

private:
  ir::RegSet defined;
  ir::RegMap<std::int64_t> constant;
};

}

#endif //MOCKER_GLOBAL_CONST_INLINE_H

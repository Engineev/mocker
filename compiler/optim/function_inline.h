#ifndef MOCKER_FUNCTION_INLINE_H
#define MOCKER_FUNCTION_INLINE_H

#include "opt_pass.h"

namespace mocker {

class FunctionInline : public ModulePass {
public:
  explicit FunctionInline(ir::Module &module);

  void operator()() override;

private:
  bool isParameter(const ir::FunctionModule &func,
                   const std::string &identifier) const;

  // Return the iterator that points to the successor block
  ir::BBLIter inlineFunction(ir::FunctionModule &caller, ir::BBLIter bbIter,
                             ir::InstListIter callInstIter);
};

} // namespace mocker

#endif // MOCKER_FUNCTION_INLINE_H

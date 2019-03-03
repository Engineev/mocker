#ifndef MOCKER_FUNCTION_INLINE_H
#define MOCKER_FUNCTION_INLINE_H

#include <unordered_set>

#include "opt_pass.h"

namespace mocker {

class FunctionInline : public ModulePass {
public:
  explicit FunctionInline(ir::Module &module);

  bool operator()() override;

private:
  void buildInlineable();

  bool isParameter(const ir::FunctionModule &func,
                   const std::string &identifier) const;

  // Return the iterator that points to the successor block
  ir::BBLIter inlineFunction(ir::FunctionModule &caller, ir::BBLIter bbIter,
                             ir::InstListIter callInstIter);

private:
  std::unordered_set<std::string> inlineable;
};

} // namespace mocker

#endif // MOCKER_FUNCTION_INLINE_H

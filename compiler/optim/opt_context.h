#ifndef MOCKER_OPT_CONTEXT_H
#define MOCKER_OPT_CONTEXT_H

#include "ir/module.h"

#include <unordered_set>

namespace mocker {

class OptContext {
public:
  explicit OptContext(ir::Module &module) : module(module) {}

  ir::Module &getModule() { return module; }

private:
  ir::Module &module;
};

} // namespace mocker

#endif // MOCKER_OPT_CONTEXT_H

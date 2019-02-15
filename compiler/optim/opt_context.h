#ifndef MOCKER_OPT_CONTEXT_H
#define MOCKER_OPT_CONTEXT_H

#include "ir/module.h"

#include <unordered_set>

namespace mocker {

class OptContext {
public:
  explicit OptContext(ir::Module &module) : module(module) {}

  ir::Module &getModule() { return module; }

  template <class Pass> bool checkPrerequisites() const {
    auto &Prerequisites = Pass::Prerequisites();
    for (auto &p : Prerequisites) {
      if (executedPass.find(p) == executedPass.end())
        return false;
    }
    return true;
  }

  template <class Pass> void updateContext() {
    executedPass.emplace(Pass::PassID());
  }

private:
  ir::Module &module;
  std::unordered_set<std::string> executedPass;
};

} // namespace mocker

#endif // MOCKER_OPT_CONTEXT_H

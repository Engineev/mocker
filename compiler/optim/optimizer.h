#ifndef MOCKER_OPTIMIZER_H
#define MOCKER_OPTIMIZER_H

#include "ir/module.h"
#include "opt_pass.h"

#include <memory>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace mocker {

class Optimizer {
public:
  explicit Optimizer(ir::Module &irModule) : irModule(irModule) {}

  template <class Pass> Optimizer &addPass() {
    if (std::is_base_of<FuncPass, Pass>::value) {
      for (auto &func : irModule.getFuncs()) {
        if (func.second.isExternalFunc())
          continue;
        passes.emplace_back(std::make_shared<Pass>(func.second));
      }
    }
    return *this;
  }

  void run() {
    for (auto &pass : passes) {
      assert(checkPrerequisites(*pass));
      if (auto p = std::dynamic_pointer_cast<FuncPass>(pass)) {
        (*p)();
        executedPasses.emplace(p->passID());
      }
    }
  }

private:
  bool checkPrerequisites(const OptPass &pass) const {
    const auto &pre = pass.prerequisites();
    for (const auto &p : pre) {
      if (executedPasses.find(p) == executedPasses.end())
        return false;
    }
    return true;
  }

private:
  ir::Module &irModule;
  std::vector<std::shared_ptr<OptPass>> passes;
  std::unordered_set<std::string> executedPasses;
};

} // namespace mocker

#endif // MOCKER_OPTIMIZER_H

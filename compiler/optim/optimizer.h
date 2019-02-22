#ifndef MOCKER_OPTIMIZER_H
#define MOCKER_OPTIMIZER_H

#include "ir/module.h"
#include "opt_context.h"
#include "opt_pass.h"

#include <cassert>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace mocker {
namespace detail {

template <class Pass> void runOptPassImpl(OptContext &ctx, BasicBlockPass *) {
  for (auto &func : ctx.getModule().getFuncs()) {
    if (func.second.isExternalFunc())
      continue;
    for (auto &bb : func.second.getMutableBBs()) {
      Pass{bb}();
    }
  }
}

template <class Pass> void runOptPassImpl(OptContext &ctx, FuncPass *) {
  for (auto &func : ctx.getModule().getFuncs()) {
    if (func.second.isExternalFunc())
      continue;
    Pass{func.second}();
    func.second.buildContext();
  }
}

template <class Pass> void runOptPassImpl(OptContext &ctx, ModulePass *) {
  Pass{ctx.getModule()}();
}

} // namespace detail

template <class Pass> void runOptPasses(OptContext &ctx) {
  detail::runOptPassImpl<Pass>(ctx, (Pass *)(nullptr));
}

} // namespace mocker

#endif // MOCKER_OPTIMIZER_H

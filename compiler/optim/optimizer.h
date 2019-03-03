#ifndef MOCKER_OPTIMIZER_H
#define MOCKER_OPTIMIZER_H

#include "ir/module.h"
#include "opt_pass.h"

#include <cassert>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace mocker {
namespace detail {

template <class Pass>
bool runOptPassImpl(ir::Module &module, BasicBlockPass *) {
  auto res = false;
  for (auto &func : module.getFuncs()) {
    if (func.second.isExternalFunc())
      continue;
    for (auto &bb : func.second.getMutableBBs()) {
      res |= Pass{bb}();
    }
  }
  return res;
}

template <class Pass> bool runOptPassImpl(ir::Module &module, FuncPass *) {
  auto res = false;
  for (auto &func : module.getFuncs()) {
    if (func.second.isExternalFunc())
      continue;
    res |= Pass{func.second}();
  }
  return res;
}

template <class Pass> bool runOptPassImpl(ir::Module &module, ModulePass *) {
  return Pass{module}();
}

} // namespace detail

template <class Pass> bool runOptPasses(ir::Module &module) {
  for (auto &func : module.getFuncs())
    func.second.buildContext();
  auto res = detail::runOptPassImpl<Pass>(module, (Pass *)(nullptr));
  for (auto &func : module.getFuncs())
    func.second.buildContext();
  ir::verifyModule(module);
  return res;
}

} // namespace mocker

#endif // MOCKER_OPTIMIZER_H

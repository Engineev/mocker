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

template <class Pass, class... Args>
bool runOptPassImpl(ir::Module &module, BasicBlockPass *, Args &&... args) {
  auto res = false;
  for (auto &func : module.getFuncs()) {
    if (func.second.isExternalFunc())
      continue;
    for (auto &bb : func.second.getMutableBBs()) {
      res |= Pass{bb, std::forward<Args>(args)...}();
    }
  }
  return res;
}

template <class Pass, class... Args>
bool runOptPassImpl(ir::Module &module, FuncPass *, Args &&... args) {
  auto res = false;
  for (auto &func : module.getFuncs()) {
    if (func.second.isExternalFunc())
      continue;
    res |= Pass{func.second, std::forward<Args>(args)...}();
  }
  return res;
}

template <class Pass, class... Args>
bool runOptPassImpl(ir::Module &module, ModulePass *, Args &&... args) {
  return Pass{module, std::forward<Args>(args)...}();
}

} // namespace detail

template <class Pass, class... Args>
bool runOptPasses(ir::Module &module, Args &&... args) {
  for (auto &func : module.getFuncs())
    func.second.buildContext();
  auto res = detail::runOptPassImpl<Pass>(module, (Pass *)(nullptr),
                                          std::forward<Args>(args)...);
  for (auto &func : module.getFuncs())
    func.second.buildContext();
  ir::verifyModule(module);
  return res;
}

} // namespace mocker

#endif // MOCKER_OPTIMIZER_H

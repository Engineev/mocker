#ifndef MOCKER_STATS_H
#define MOCKER_STATS_H

#include <cstddef>
#include <type_traits>

#include "module.h"

namespace mocker {
namespace ir {

class Stats {
public:
  explicit Stats(const Module &module) : module(module) {}

  std::size_t countBBs() const {
    std::size_t res = 0;
    for (auto &f : module.getFuncs())
      res += f.second.getBBs().size();
    return res;
  }

  template <class Condition>
  std::size_t countInstsIf(const Condition &condition) const {
    std::size_t res = 0;
    for (auto &f : module.getFuncs())
      res += countInstsIf(condition, f.second);
    return res;
  }

  template <class Inst> std::size_t countInsts() const {
    return countInstsIf([&](const std::shared_ptr<ir::IRInst> &inst) {
      return (bool)std::dynamic_pointer_cast<Inst>(inst);
    });
  }

  template <class Inst>
  std::size_t countInsts(const std::string &funcName) const {
    return countInsts<Inst>(module.getFuncs().at(funcName));
  }

private:
  template <class Condition>
  std::size_t countInstsIf(const Condition &condition,
                           const FunctionModule &func) const {
    std::size_t res = 0;
    for (const auto &bb : func.getBBs()) {
      for (const auto &inst : bb.getInsts()) {
        if (condition(inst))
          ++res;
      }
    }
    return res;
  }

private:
  const Module &module;
};

} // namespace ir
} // namespace mocker

#endif // MOCKER_STATS_H

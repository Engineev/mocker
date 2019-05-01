#ifndef MOCKER_NASM_STATS_H
#define MOCKER_NASM_STATS_H

#include <cstddef>
#include <type_traits>

#include "module.h"

namespace mocker {
namespace nasm {

class Stats {
public:
  explicit Stats(const Module &module) : module(module) {}

  std::size_t countJumps() const {
    return countInstsIf([](const auto &inst) {
      return dyc<CJump>(inst) || dyc<Jmp>(inst) || dyc<Ret>(inst);
    });
  }

  std::size_t countMemOperation() const {
    return countInstsIf(
        [](const auto &inst) { return !getInvolvedMem(inst).empty(); });
  }

  template <class Condition>
  std::size_t countInstsIf(const Condition &condition) const {
    std::size_t res = 0;
    for (auto &l : module.getSection(".text").getLines()) {
      auto &inst = l.inst;
      if (!inst)
        continue;
      if (condition(inst))
        ++res;
    }
    return res;
  }

  std::size_t countInsts() const {
    return countInstsIf([&](const auto &inst) { return true; });
  }

private:
  const Module &module;
};

} // namespace nasm
} // namespace mocker

#endif // MOCKER_NASM_STATS_H

#include "module_simplification.h"

#include <cassert>
#include <queue>
#include <unordered_set>

#include "ir/helper.h"

namespace mocker {

bool UnusedFunctionRemoval::operator()() {
  std::unordered_set<std::string> used;
  used.emplace("main");

  std::queue<std::string> workList;
  workList.push("main");

  while (!workList.empty()) {
    auto funcName = workList.front();
    workList.pop();

    const auto &func = module.getFuncs().at(funcName);
    if (func.isExternalFunc())
      continue;
    for (auto &bb : func.getBBs()) {
      for (auto &inst : bb.getInsts()) {
        auto call = ir::dyc<ir::Call>(inst);
        if (!call)
          continue;
        auto callee = call->getFuncName();
        if (used.find(callee) != used.end())
          continue;
        used.emplace(callee);
        workList.push(callee);
      }
    }
  }

  std::size_t cnt = 0;
  ir::Module::FuncsMap funcs;
  for (auto &kv : module.getFuncs()) {
    if (used.find(kv.first) == used.end()) {
      ++cnt;
      continue;
    }
    funcs.emplace(kv.first, std::move(kv.second));
  }
  module.getFuncs() = std::move(funcs);
  return cnt != 0;
}

} // namespace mocker

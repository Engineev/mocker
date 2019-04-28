#include "func_global_var.h"

#include <queue>

#include "ir/helper.h"
#include "set_operation.h"

namespace mocker {
namespace {

// Return the functions called by [func].
std::vector<std::string> collectCallees(const ir::FunctionModule &func) {
  std::vector<std::string> res;

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto call = ir::dyc<ir::Call>(inst);
      if (!call)
        continue;
      res.emplace_back(call->getFuncName());
    }
  }

  return res;
}

// Return the map that maps each function to the functions that calls it.
std::unordered_map<std::string, std::vector<std::string>>
buildCallGraph(const ir::Module &module) {
  std::unordered_map<std::string, std::vector<std::string>> res;
  for (auto &kv : module.getFuncs())
    res[kv.first] = {};

  for (auto &kv : module.getFuncs()) {
    auto &func = kv.second;
    auto callees = collectCallees(func);
    for (auto &callee : callees) {
      res[callee].emplace_back(kv.first);
    }
  }

  return res;
}

} // namespace

void FuncGlobalVar::init(const ir::Module &module) {
  const auto CallGraph = buildCallGraph(module);

  // initialize
  for (auto &kv : module.getFuncs()) {
    use[kv.first] = def[kv.first] = {};
    auto &func = kv.second;
    auto &fUse = use[kv.first];
    auto &fDef = def[kv.first];

    auto update = [&fUse, &fDef](const std::shared_ptr<ir::IRInst> &inst) {
      if (auto p = ir::dyc<ir::Store>(inst)) {
        if (auto reg = ir::dycGlobalReg(p->getAddr()))
          fDef.emplace(reg->getIdentifier());
        if (auto reg = ir::dycGlobalReg(p->getVal()))
          fUse.emplace(reg->getIdentifier());
        return;
      }

      auto operands = ir::getOperandsUsed(inst);
      for (auto &operand : operands) {
        if (auto reg = ir::dycGlobalReg(operand))
          fUse.emplace(reg->getIdentifier());
      }
    };

    for (auto &bb : func.getBBs()) {
      for (auto &inst : bb.getInsts())
        update(inst);
    }
  }

  // update the result iteratively
  std::queue<std::string> worklist;
  for (auto &kv : module.getFuncs())
    worklist.emplace(kv.first);

  while (!worklist.empty()) {
    auto funcName = worklist.front();
    worklist.pop();
    const auto &Callers = CallGraph.at(funcName);
    const auto &usedByCallee = use.at(funcName);
    const auto &defedByCallee = def.at(funcName);
    for (auto &caller : Callers) {
      auto &usedByCaller = use.at(caller);
      std::size_t originalSize = usedByCaller.size();
      unionSet(usedByCaller, usedByCallee);
      std::size_t newSize = usedByCaller.size();
      if (originalSize != newSize)
        worklist.emplace(caller);

      auto &defedByCaller = def.at(caller);
      originalSize = defedByCaller.size();
      unionSet(defedByCaller, defedByCallee);
      newSize = defedByCaller.size();
      if (originalSize != newSize)
        worklist.emplace(caller);
    }
  }
}

std::unordered_set<std::string>
FuncGlobalVar::getInvolved(const std::string &funcName) const {
  auto res = use.at(funcName);
  unionSet(res, def.at(funcName));
  return res;
}

} // namespace mocker

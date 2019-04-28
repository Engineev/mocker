#include "promote_global_variables.h"

#include <cassert>
#include <queue>
#include <vector>

#include "helper.h"
#include "ir/helper.h"
#include "ir/ir_inst.h"

#include <iostream>

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

template <class Set, class T> void unionSet(Set &s, const T &xs) {
  for (auto &x : xs) {
    s.emplace(x);
  }
}

// Return the map that maps each function to the global variables it used
std::unordered_map<std::string, std::unordered_set<std::string>>
buildGlobalVarUsed(
    const ir::Module &module,
    const std::unordered_map<std::string, std::vector<std::string>>
        &CallGraph) {
  std::unordered_map<std::string, std::unordered_set<std::string>> res;
  // Initialize res
  for (auto &kv : module.getFuncs()) {
    auto &func = kv.second;
    auto &used = res[kv.first];
    for (auto &bb : func.getBBs()) {
      for (auto &inst : bb.getInsts()) {
        auto operands = ir::getOperandsUsed(inst);
        for (auto &operand : operands) {
          auto reg = ir::dycGlobalReg(operand);
          if (reg && reg->getIdentifier() != "@null")
            used.insert(reg->getIdentifier());
        }
      }
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
    const auto &usedByCallee = res[funcName];
    for (auto &caller : Callers) {
      auto &usedByCaller = res[caller];
      std::size_t originalSize = usedByCaller.size();
      unionSet(usedByCaller, usedByCallee);
      std::size_t newSize = usedByCaller.size();
      if (originalSize != newSize)
        worklist.emplace(caller);
    }
  }

  return res;
}

} // namespace
} // namespace mocker

namespace mocker {

PromoteGlobalVariables::PromoteGlobalVariables(ir::Module &module)
    : ModulePass(module) {}

bool PromoteGlobalVariables::operator()() {
  const auto CallGraph = buildCallGraph(module);
  auto GlobalVarUsed = buildGlobalVarUsed(module, CallGraph);

  for (auto &kv : module.getFuncs())
    if (!kv.second.isExternalFunc())
      promoteGlobalVariables(kv.second, GlobalVarUsed);
  return false;
}

void PromoteGlobalVariables::promoteGlobalVariables(
    ir::FunctionModule &func,
    const std::unordered_map<std::string, std::unordered_set<std::string>>
        &GlobalVarUsed) {
  auto GlobalVars = GlobalVarUsed.at(func.getIdentifier());
  std::unordered_map<std::string, std::shared_ptr<ir::Reg>> aliasReg;
  for (const auto &name : GlobalVars) {
    assert(name != "@null");
    aliasReg[name] = func.makeTempLocalReg("alias" + name);
  }

  // Rename the use
  // * In some cases, the use can not be renamed. I am not sure whether I have
  //   taken all such cases into consideration here.
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      const auto Operands = ir::getOperandsUsed(inst);
      auto newOperands = Operands;
      for (auto &operand : newOperands) {
        auto p = ir::dycGlobalReg(operand);
        if (p && p->getIdentifier() != "@null")
          operand = aliasReg.at(p->getIdentifier());
      }
      if (ir::dyc<ir::Store>(inst)) {
        newOperands.at(1) = Operands.at(1);
      }
      inst = ir::copyWithReplacedOperands(inst, newOperands);
    }
  }

  // Insert Alloca's
  auto &firstBB = *func.getFirstBB();
  for (auto &nameReg : aliasReg) {
    std::list<std::shared_ptr<ir::IRInst>> toBeInserted;
    auto gReg = std::make_shared<ir::Reg>(nameReg.first);
    toBeInserted.emplace_back(std::make_shared<ir::Alloca>(nameReg.second));
    auto tmp = func.makeTempLocalReg();
    toBeInserted.emplace_back(std::make_shared<ir::Load>(tmp, gReg));
    toBeInserted.emplace_back(std::make_shared<ir::Store>(nameReg.second, tmp));
    firstBB.getMutableInsts().splice(firstBB.getMutableInsts().begin(),
                                     std::move(toBeInserted));
  }

  // Insert Loads & Stores before Calls and Rets
  for (auto &bb : func.getMutableBBs()) {
    auto &insts = bb.getMutableInsts();
    auto insert = [&insts, &func, &aliasReg](ir::InstListIter iter,
                                             const std::string &name) {
      auto gReg = std::make_shared<ir::Reg>(name);
      auto alias = aliasReg.at(name);
      auto tmp = func.makeTempLocalReg();
      insts.insert(iter, std::make_shared<ir::Load>(tmp, alias));
      insts.insert(iter, std::make_shared<ir::Store>(gReg, tmp));
      if (ir::dyc<ir::Ret>(*iter))
        return;
      ++iter;
      auto tmp2 = func.makeTempLocalReg();
      insts.insert(iter, std::make_shared<ir::Load>(tmp2, gReg));
      insts.insert(iter, std::make_shared<ir::Store>(alias, tmp2));
    };

    for (auto iter = insts.begin(); iter != insts.end(); ++iter) {
      if (ir::dyc<ir::Ret>(*iter)) {
        for (auto &toBeStored : GlobalVars)
          insert(iter, toBeStored);
        continue;
      }

      if (auto call = ir::dyc<ir::Call>(*iter)) {
        auto &usedByCallee = GlobalVarUsed.at(call->getFuncName());
        for (auto &toBeStored : usedByCallee) {
          if (GlobalVars.find(toBeStored) == GlobalVars.end())
            continue;
          insert(iter, toBeStored);
        }
        continue;
      }
    }
  }
}

} // namespace mocker
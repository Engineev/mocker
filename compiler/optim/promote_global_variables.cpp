#include "promote_global_variables.h"

#include <cassert>

#include "helper.h"
#include "ir/helper.h"

namespace mocker {

PromoteGlobalVariables::PromoteGlobalVariables(ir::Module &module)
    : ModulePass(module) {
  for (auto &kv : module.getFuncs()) {
    if (kv.second.isExternalFunc())
      continue;
    buildGlobalVarUsedImpl(kv.second);
  }
}

void PromoteGlobalVariables::operator()() {
  for (auto &kv : module.getFuncs())
    if (!kv.second.isExternalFunc())
      promoteGlobalVariables(kv.second);
}

void PromoteGlobalVariables::buildGlobalVarUsedImpl(
    const ir::FunctionModule &func) {
  if (globalVarUsed.find(func.getIdentifier()) != globalVarUsed.end())
    return;

  auto &used = globalVarUsed[func.getIdentifier()];
  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      if (auto call = ir::dyc<ir::Call>(inst)) {
        auto &callee = module.getFuncs().at(call->getFuncName());
        if (!callee.isExternalFunc()) {
          buildGlobalVarUsedImpl(callee);
          for (auto &ident : globalVarUsed.at(callee.getIdentifier()))
            used.emplace(ident);
        }
      }

      auto operands = ir::getOperandsUsed(inst);
      for (auto &operand : operands) {
        auto reg = ir::cdyc<ir::GlobalReg>(operand);
        if (reg && reg->identifier != "@null")
          used.insert(reg->identifier);
      }
    }
  }
}

void PromoteGlobalVariables::promoteGlobalVariables(ir::FunctionModule &func) {
  const auto &globalVars = globalVarUsed[func.getIdentifier()];
  std::unordered_map<std::string, std::shared_ptr<ir::LocalReg>> aliasReg;
  for (const auto &name : globalVars) {
    assert(name != "@null");
    aliasReg[name] = func.makeTempLocalReg("alias" + name);
  }

  // Rename the use
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      auto operands = ir::getOperandsUsed(inst);
      for (auto &operand : operands) {
        auto p = ir::cdyc<ir::GlobalReg>(operand);
        if (p && p->identifier != "@null")
          operand = aliasReg.at(p->identifier);
      }
      inst = ir::copyWithReplacedOperands(inst, operands);
    }
  }

  // Insert Alloca's
  auto &firstBB = *func.getFirstBB();
  for (auto &nameReg : aliasReg) {
    std::list<std::shared_ptr<ir::IRInst>> toBeInserted;
    auto gReg = std::make_shared<ir::GlobalReg>(nameReg.first);
    toBeInserted.emplace_back(std::make_shared<ir::Alloca>(nameReg.second, 8));
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
      auto gReg = std::make_shared<ir::GlobalReg>(name);
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
        for (auto &toBeStored : globalVars)
          insert(iter, toBeStored);
        continue;
      }

      if (auto call = ir::dyc<ir::Call>(*iter)) {
        auto &usedByCallee = globalVarUsed[call->getFuncName()];
        for (auto &toBeStored : usedByCallee) {
          if (globalVars.find(toBeStored) == globalVars.end())
            continue;
          insert(iter, toBeStored);
        }
        continue;
      }
    }
  }
}

} // namespace mocker
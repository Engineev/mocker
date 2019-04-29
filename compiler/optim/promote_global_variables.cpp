#include "promote_global_variables.h"

#include <cassert>
#include <vector>

#include "helper.h"
#include "ir/helper.h"
#include "ir/ir_inst.h"
#include "set_operation.h"

namespace mocker {

PromoteGlobalVariables::PromoteGlobalVariables(ir::Module &module)
    : ModulePass(module) {}

bool PromoteGlobalVariables::operator()() {
  funcGlobalVar.init(module);

  for (auto &kv : module.getFuncs())
    if (!kv.second.isExternalFunc())
      promoteGlobalVariables(kv.second);
  return false;
}

void PromoteGlobalVariables::promoteGlobalVariables(ir::FunctionModule &func) {
  const auto &Use = funcGlobalVar.getUse(func.getIdentifier());
  const auto &Def = funcGlobalVar.getDef(func.getIdentifier());

  std::unordered_map<std::string, std::shared_ptr<ir::Reg>> aliasReg;
  for (const auto &name : funcGlobalVar.getInvolved(func.getIdentifier())) {
    assert(name != "@null");
    aliasReg[name] = func.makeTempLocalReg("alias" + name);
  }

  // Rename the load & store
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {

      if (auto p = ir::dyc<ir::Store>(inst)) {
        auto reg = ir::dycGlobalReg(p->getAddr());
        if (!reg)
          continue;
        reg = aliasReg.at(reg->getIdentifier());
        inst = std::make_shared<ir::Store>(reg, p->getVal());
        continue;
      }
      if (auto p = ir::dyc<ir::Load>(inst)) {
        auto reg = ir::dycGlobalReg(p->getAddr());
        if (!reg)
          continue;
        reg = aliasReg.at(reg->getIdentifier());
        inst = std::make_shared<ir::Load>(p->getDest(), reg);
        continue;
      }
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

    auto reStore = [&insts, &func, &aliasReg](ir::InstListIter iter,
                                              const std::string &name) {
      if (name == "@null")
        return;
      auto gReg = std::make_shared<ir::Reg>(name);
      auto alias = aliasReg.at(name);
      auto tmp = func.makeTempLocalReg();
      insts.insert(iter, std::make_shared<ir::Load>(tmp, alias));
      insts.insert(iter, std::make_shared<ir::Store>(gReg, tmp));
    };

    auto reLoad = [&insts, &func, &aliasReg](ir::InstListIter iter,
                                             const std::string &name) {
      if (name == "@null")
        return;
      auto gReg = std::make_shared<ir::Reg>(name);
      auto alias = aliasReg.at(name);
      ++iter;
      auto tmp = func.makeTempLocalReg();
      insts.insert(iter, std::make_shared<ir::Load>(tmp, gReg));
      insts.insert(iter, std::make_shared<ir::Store>(alias, tmp));
    };

    for (auto iter = insts.begin(); iter != insts.end(); ++iter) {
      if (ir::dyc<ir::Ret>(*iter)) {
        for (auto &toBeStored : Def)
          reStore(iter, toBeStored);
        continue;
      }

      if (auto call = ir::dyc<ir::Call>(*iter)) {
        const auto &usedByCallee = funcGlobalVar.getUse(call->getFuncName());
        for (auto &toBeStored : intersectSets(Def, usedByCallee)) {
          reStore(iter, toBeStored);
        }

        const auto &defedByCallee = funcGlobalVar.getDef(call->getFuncName());
        for (auto &reg : intersectSets(Use, defedByCallee)) {
          reLoad(iter, reg);
        }

        continue;
      }
    }
  }
}

} // namespace mocker
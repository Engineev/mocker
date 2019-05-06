#include "func_attr.h"

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

void FuncAttr::init(const ir::Module &module) {
  globalVarUses.clear();
  globalVarDefs.clear();
  pureFuncs.clear();

  buildGlobalVarInfo(module);

  std::size_t size = pureFuncs.size();
  while (true) {
    for (auto &kv : module.getFuncs()) {
      if (kv.second.isExternalFunc())
        continue;
      if (isPureFunc(kv.second))
        pureFuncs.emplace(kv.first);
    }
    if (size == pureFuncs.size())
      break;
    size = pureFuncs.size();
  }
}

std::unordered_set<std::string>
FuncAttr::getInvolvedGlobalVars(const std::string &funcName) const {
  auto res = globalVarUses.at(funcName);
  unionSet(res, globalVarDefs.at(funcName));
  return res;
}

void FuncAttr::buildGlobalVarInfo(const ir::Module &module) {
  const auto CallGraph = buildCallGraph(module);

  // initialize
  for (auto &kv : module.getFuncs()) {
    globalVarUses[kv.first] = globalVarDefs[kv.first] = {};
    auto &func = kv.second;
    auto &fUse = globalVarUses[kv.first];
    auto &fDef = globalVarDefs[kv.first];

    auto update = [&fUse, &fDef](const std::shared_ptr<ir::IRInst> &inst) {
      if (auto p = ir::dyc<ir::Store>(inst)) {
        if (auto reg = ir::dycGlobalReg(p->getAddr()))
          fDef.emplace(reg->getIdentifier());
        return;
      }
      if (auto p = ir::dyc<ir::Load>(inst)) {
        if (auto reg = ir::dycGlobalReg(p->getAddr()))
          fUse.emplace(reg->getIdentifier());
        return;
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
    const auto &usedByCallee = globalVarUses.at(funcName);
    const auto &defedByCallee = globalVarDefs.at(funcName);
    for (auto &caller : Callers) {
      auto &usedByCaller = globalVarUses.at(caller);
      std::size_t originalSize = usedByCaller.size();
      unionSet(usedByCaller, usedByCallee);
      std::size_t newSize = usedByCaller.size();
      if (originalSize != newSize)
        worklist.emplace(caller);

      auto &defedByCaller = globalVarDefs.at(caller);
      originalSize = defedByCaller.size();
      unionSet(defedByCaller, defedByCallee);
      newSize = defedByCaller.size();
      if (originalSize != newSize)
        worklist.emplace(caller);
    }
  }
}

bool FuncAttr::isPureFunc(const ir::FunctionModule &func) {
  std::unordered_set<std::string> stackVar;
  for (auto &inst : func.getBasicBlock(func.getFirstBBLabel()).getInsts()) {
    auto alloca = ir::dyc<ir::Alloca>(inst);
    if (!alloca)
      continue;
    stackVar.emplace(alloca->getDest()->getIdentifier());
  }

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      if (auto p = ir::dyc<ir::Store>(inst)) {
        auto reg = ir::dycLocalReg(p->getAddr());
        if (!reg || !isIn(stackVar, reg->getIdentifier()))
          return false;
      }
      if (auto p = ir::dyc<ir::Load>(inst)) {
        auto reg = ir::dycLocalReg(p->getAddr());
        if (!reg || !isIn(stackVar, reg->getIdentifier()))
          return false;
      }
      if (auto call = ir::dyc<ir::Call>(inst)) {
        if (call->getFuncName() == func.getIdentifier())
          continue;
        if (!isIn(pureFuncs, call->getFuncName()))
          return false;
      }
    }
  }
  return true;
}

} // namespace mocker

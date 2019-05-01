#include "global_const_inline.h"

#include <unordered_set>

#include "helper.h"
#include "ir/helper.h"
#include "set_operation.h"

namespace mocker {

bool GlobalConstantInline::operator()() {
  for (auto &kv : module.getFuncs())
    checkFunc(kv.second);
  bool res = false;
  for (auto &kv : module.getFuncs())
    res |= rewrite(kv.second);
  for (auto &kv : module.getFuncs())
    removeDeletedInsts(kv.second);
  return res;
}

void GlobalConstantInline::checkFunc(const ir::FunctionModule &func) {
  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto store = ir::dyc<ir::Store>(inst);
      if (!store)
        continue;
      auto globalReg = ir::dycGlobalReg(store->getAddr());
      if (!globalReg)
        continue;

      bool firstTime = !isIn(defined, globalReg);
      defined.emplace(globalReg);
      if (!firstTime && !isIn(constant, globalReg))
        continue;

      auto lit = ir::dyc<ir::IntLiteral>(store->getVal());
      if (!lit) {
        if (isIn(constant, globalReg))
          constant.erase(constant.find(globalReg));
        continue;
      }

      if (!firstTime && constant.at(globalReg) != lit->getVal()) {
        constant.erase(constant.find(globalReg));
        continue;
      }
      constant[globalReg] = lit->getVal();
    }
  }
}

bool GlobalConstantInline::rewrite(ir::FunctionModule &func) {
  bool modified = false;
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      if (auto store = ir::dyc<ir::Store>(inst)) {
        auto globalReg = ir::dycGlobalReg(store->getAddr());
        if (!globalReg || !isIn(constant, globalReg))
          continue;
        inst = std::make_shared<ir::Deleted>();
        continue;
      }

      auto load = ir::dyc<ir::Load>(inst);
      if (!load)
        continue;
      auto globalReg = ir::dycGlobalReg(load->getAddr());
      if (!globalReg || !isIn(constant, globalReg))
        continue;
      auto lit = std::make_shared<ir::IntLiteral>(constant.at(globalReg));
      inst = std::make_shared<ir::Assign>(load->getDest(), lit);
      modified = true;
    }
  }
  return modified;
}

} // namespace mocker

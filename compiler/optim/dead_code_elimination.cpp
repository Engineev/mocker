#include "dead_code_elimination.h"

#include <cctype>
#include <iostream>

#include "helper.h"
#include "ir/helper.h"

namespace mocker {

DeadCodeElimination::DeadCodeElimination(ir::FunctionModule &func)
    : FuncPass(func) {
  instDefine = buildInstDefine(func);
}

bool DeadCodeElimination::operator()() {
  init();
  mark();
  sweep();
  return cnt != 0;
}

bool DeadCodeElimination::isParameter(const std::string &identifier) {
  if (!std::isdigit(identifier[0]))
    return false;

  std::size_t ed = 0;
  auto n = std::stoll(identifier, &ed);
  if (ed != identifier.size())
    return false;
  return n < (int)func.getArgs().size();
}

void DeadCodeElimination::updateWorkList(
    const std::shared_ptr<ir::IRInst> &inst) {
  auto operands = ir::getOperandsUsed(inst);
  for (auto &operand : operands) {
    if (auto p = ir::dycLocalReg(operand))
      workList.emplace(p->getIdentifier());
  }
}

void DeadCodeElimination::init() {
  auto isUseful = [](const std::shared_ptr<ir::IRInst> &inst) -> bool {
    return ir::dyc<ir::Terminator>(inst) || ir::dyc<ir::Store>(inst) ||
           ir::dyc<ir::Call>(inst);
  };

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      if (!isUseful(inst))
        continue;

      useful.emplace(inst->getID());
      updateWorkList(inst);
    }
  }
}

void DeadCodeElimination::mark() {
  while (!workList.empty()) {
    auto reg = workList.front();
    workList.pop();
    if (reg == ".phi_nan" || isParameter(reg))
      continue;
    auto inst = instDefine.at(reg);
    if (useful.find(inst->getID()) != useful.end())
      continue;
    useful.emplace(inst->getID());
    updateWorkList(inst);
  }
}

void DeadCodeElimination::sweep() {
  for (auto &bb : func.getMutableBBs()) {
    ir::InstList newInsts;
    for (auto &inst : bb.getInsts())
      if (useful.find(inst->getID()) != useful.end())
        newInsts.emplace_back(inst);
    cnt += bb.getInsts().size() - newInsts.size();
    bb.getMutableInsts() = std::move(newInsts);
  }

  //  std::cerr << "DeadCodeElimination: eliminate " << cnt << " insts in "
  //            << func.getIdentifier() << std::endl;
}

} // namespace mocker
#include "dead_code_elimination.h"

#include <cctype>
#include <iostream>

#include "analysis/dominance.h"
#include "helper.h"
#include "ir/helper.h"
#include "set_operation.h"

namespace mocker {

DeadCodeElimination::DeadCodeElimination(ir::FunctionModule &func)
    : FuncPass(func) {}

bool DeadCodeElimination::operator()() {
  rdt.init(func, true);
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

void DeadCodeElimination::init() {
  auto isCritical = [](const std::shared_ptr<ir::IRInst> &inst) -> bool {
    return ir::dyc<ir::Ret>(inst) || ir::dyc<ir::Store>(inst) ||
           ir::dyc<ir::Call>(inst);
  };

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      residingBB[inst->getID()] = bb.getLabelID();
      if (!isCritical(inst))
        continue;
      useful.emplace(inst->getID());
      worklist.emplace(inst);
    }
  }
}

void DeadCodeElimination::mark() {
  std::unordered_map<std::string, std::shared_ptr<ir::IRInst>> instDefine =
      buildInstDefine(func);

  auto markUsefulBB = [this](std::size_t label) {
    auto &bb = func.getBasicBlock(label);
    auto terminator = bb.getInsts().back();
    if (isIn(useful, terminator->getID()))
      return;
    useful.emplace(terminator->getID());
    worklist.emplace(terminator);
  };

  while (!worklist.empty()) {
    auto inst = worklist.front();
    worklist.pop();
    markUsefulBB(residingBB.at(inst->getID()));
    for (auto &operand : ir::getOperandsUsed(inst)) {
      auto reg = ir::dycLocalReg(operand);
      if (!reg || reg->getIdentifier() == ".phi_nan" ||
          isParameter(reg->getIdentifier()))
        continue;
      auto defingInst = instDefine.at(reg->getIdentifier());
      if (isIn(useful, defingInst->getID()))
        continue;
      useful.emplace(defingInst->getID());
      worklist.emplace(defingInst);
    }
    if (auto phi = ir::dyc<ir::Phi>(inst)) {
      for (auto &option : phi->getOptions()) {
        auto label = option.second->getID();
        markUsefulBB(label);
      }
    }

    auto curBB = residingBB.at(inst->getID());
    for (auto label : rdt.getDominanceFrontier(curBB)) {
      markUsefulBB(label);
    }
  }
}

void DeadCodeElimination::sweep() {
  for (auto &bb : func.getMutableBBs()) {
    ir::InstList newInsts;
    for (auto &inst : bb.getInsts()) {
      if (isIn(useful, inst->getID()) || ir::dyc<ir::Jump>(inst)) {
        newInsts.emplace_back(inst);
        continue;
      }
      auto br = ir::dyc<ir::Branch>(inst);
      if (!br)
        continue;
      auto target = rdt.getImmediateDominator(bb.getLabelID());
      while (
          !isIn(useful, func.getBasicBlock(target).getInsts().back()->getID()))
        target = rdt.getImmediateDominator(target);
      newInsts.emplace_back(
          std::make_shared<ir::Jump>(std::make_shared<ir::Label>(target)));
    }

    cnt += bb.getInsts().size() - newInsts.size();
    bb.getMutableInsts() = std::move(newInsts);
  }
}

} // namespace mocker
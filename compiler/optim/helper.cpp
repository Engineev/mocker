#include "helper.h"

#include <cassert>

namespace mocker {

std::unordered_map<std::string, std::shared_ptr<ir::IRInst>>
buildInstDefine(const ir::FunctionModule &func) {
  std::unordered_map<std::string, std::shared_ptr<ir::IRInst>> res;

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto dest = ir::getDest(inst);
      if (!dest)
        continue;
      auto p = ir::dycLocalReg(dest);
      assert(p);
      res[p->getIdentifier()] = inst;
    }
  }

  return res;
}

std::unordered_map<std::size_t, std::vector<std::size_t>>
buildBlockPredecessors(const ir::FunctionModule &func) {
  std::unordered_map<std::size_t, std::vector<std::size_t>> res;

  for (auto &bb : func.getBBs()) {
    auto succ = bb.getSuccessors();
    for (auto &s : succ)
      res[s].emplace_back(bb.getLabelID());
  }

  return res;
}

void removeInstIf(
    ir::FunctionModule &func,
    std::function<bool(const std::shared_ptr<ir::IRInst> &)> condition) {
  for (auto &bb : func.getMutableBBs()) {
    ir::InstList newInsts;
    for (auto &inst : bb.getMutableInsts()) {
      if (!condition(inst))
        newInsts.emplace_back(std::move(inst));
    }
    bb.getMutableInsts() = std::move(newInsts);
  }
}

void removeDeletedInsts(ir::FunctionModule &func) {
  removeInstIf(func, [](const std::shared_ptr<ir::IRInst> &inst) {
    return (bool)ir::dyc<ir::Deleted>(inst);
  });
}

std::shared_ptr<ir::IRInst> deletePhiOption(const std::shared_ptr<ir::Phi> &phi,
                                            std::size_t bbLabel) {
  std::vector<ir::Phi::Option> newOptions;

  for (auto &option : phi->getOptions())
    if (option.second->getID() != bbLabel) {
      newOptions.emplace_back(
          std::make_pair(option.first, ir::dyc<ir::Label>(option.second)));
    }

  return std::make_shared<ir::Phi>(phi->getDest(), std::move(newOptions));
}

std::shared_ptr<ir::IRInst>
replacePhiOption(const std::shared_ptr<ir::Phi> &phi, std::size_t oldLabel,
                 std::size_t newLabel) {
  std::vector<ir::Phi::Option> newOptions;
  for (auto &option : phi->getOptions()) {
    newOptions.emplace_back(std::make_pair(
        option.first,
        std::make_shared<ir::Label>(option.second->getID() == oldLabel
                                        ? newLabel
                                        : option.second->getID())));
  }
  return std::make_shared<ir::Phi>(phi->getDest(), std::move(newOptions));
}

std::shared_ptr<ir::IRInst>
replaceTerminatorLabel(const std::shared_ptr<ir::IRInst> &inst,
                       std::size_t oldLabel, std::size_t newLabel) {
  if (auto p = ir::dyc<ir::Jump>(inst)) {
    assert(p->getLabel()->getID() == oldLabel);
    return std::make_shared<ir::Jump>(std::make_shared<ir::Label>(newLabel));
  }
  if (auto p = ir::dyc<ir::Branch>(inst)) {
    if (p->getThen()->getID() == oldLabel)
      return std::make_shared<ir::Branch>(p->getCondition(),
                                          std::make_shared<ir::Label>(newLabel),
                                          p->getElse());
    if (p->getElse()->getID() == oldLabel)
      return std::make_shared<ir::Branch>(
          p->getCondition(), p->getThen(),
          std::make_shared<ir::Label>(newLabel));
    assert(false);
  }
  assert(false);
}

} // namespace mocker
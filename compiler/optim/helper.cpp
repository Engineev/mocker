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
      auto p = std::dynamic_pointer_cast<ir::LocalReg>(dest);
      assert(p);
      res[p->identifier] = inst;
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
    return (bool)dyc<ir::Deleted>(inst);
  });
}

std::shared_ptr<ir::IRInst> deletePhiOption(const std::shared_ptr<ir::Phi> &phi,
                                            std::size_t bbLabel) {
  std::vector<ir::Phi::Option> newOptions;

  for (auto &option : phi->options)
    if (option.second->id != bbLabel)
      newOptions.emplace_back(option);

  if (newOptions.size() == 1)
    return std::make_shared<ir::Assign>(phi->dest, newOptions.at(0).first);
  return std::make_shared<ir::Phi>(phi->dest, std::move(newOptions));
}

std::shared_ptr<ir::IRInst>
replacePhiOption(const std::shared_ptr<ir::Phi> &phi, std::size_t oldLabel,
                 std::size_t newLabel) {
  auto res = phi;
  for (auto &option : res->options)
    if (option.second->id == oldLabel) {
      option.second->id = newLabel;
      break;
    }
  return res;
}

} // namespace mocker
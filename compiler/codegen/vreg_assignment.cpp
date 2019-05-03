#include "vreg_assignment.h"

#include "ir/helper.h"

namespace mocker {

void VRegAssignment::init(const ir::FunctionModule &func) {
  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto dest = ir::getDest(inst);
      if (!dest)
        continue;
      mp[dest] = std::make_shared<nasm::Register>("vir_" +
                                                  dest->getIdentifier() + "_");
    }
  }

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        continue;
      for (auto &option : phi->getOptions()) {
        auto reg = ir::dycLocalReg(option.first);
        if (!reg)
          continue;
        mp[reg] = mp[phi->getDest()];
      }
    }
  }
}

} // namespace mocker
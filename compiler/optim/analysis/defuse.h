#ifndef MOCKER_DEFUSE_H
#define MOCKER_DEFUSE_H

#include <vector>

#include "ir/helper.h"
#include "ir/module.h"

namespace mocker {

class DefUseChain {
public:
  class Use {
  public:
    Use(std::size_t bbLabel, std::shared_ptr<ir::IRInst> inst)
        : bbLabel(bbLabel), inst(std::move(inst)) {}
    Use(const Use &) = default;
    Use &operator=(const Use &) = default;
    ~Use() = default;

    const std::size_t getBBLabel() const { return bbLabel; }

    const std::shared_ptr<ir::IRInst> getInst() const { return inst; }

  private:
    std::size_t bbLabel = 0;
    std::shared_ptr<ir::IRInst> inst;
  };

  void init(const ir::FunctionModule &func) {
    for (auto &bb : func.getBBs()) {
      for (auto &inst : bb.getInsts()) {
        if (auto dest = ir::getDest(inst))
          chain[dest] = {};
      }
    }

    for (auto &bb : func.getBBs()) {
      for (auto &inst : bb.getInsts()) {
        auto operands = ir::getOperandsUsed(inst);
        for (auto &operand : operands) {
          auto reg = ir::dycLocalReg(operand);
          if (!reg)
            continue;
          chain[reg].emplace_back(bb.getLabelID(), inst);
        }
      }
    }
  }

  const std::vector<Use> getUses(const std::shared_ptr<ir::Reg> &def) const {
    return chain.at(def);
  }

private:
  ir::RegMap<std::vector<Use>> chain;
};

} // namespace mocker

namespace mocker {

class UseDefChain {
public:
  class Def {
  public:
    Def(std::size_t bbLabel, std::shared_ptr<ir::IRInst> inst)
        : bbLabel(bbLabel), inst(std::move(inst)) {}
    Def(const Def &) = default;
    Def &operator=(const Def &) = default;
    ~Def() = default;

    const std::size_t getBBLabel() const { return bbLabel; }

    const std::shared_ptr<ir::IRInst> &getInst() const { return inst; }

  private:
    std::size_t bbLabel = 0;
    std::shared_ptr<ir::IRInst> inst;
  };

  void init(const ir::FunctionModule &func) {
    for (auto &bb : func.getBBs()) {
      for (auto &inst : bb.getInsts()) {
        auto dest = ir::getDest(inst);
        if (!dest)
          continue;
        chain.emplace(std::make_pair(dest, Def(bb.getLabelID(), inst)));
      }
    }
  }

  const Def &getDef(const std::shared_ptr<ir::Reg> &reg) const {
    return chain.at(reg);
  }

private:
  ir::RegMap<Def> chain;
};

} // namespace mocker

#endif // MOCKER_DEFUSE_H

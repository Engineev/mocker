#ifndef MOCKER_INDUCTION_VARIABLE_H
#define MOCKER_INDUCTION_VARIABLE_H

#include "analysis/defuse.h"
#include "analysis/func_attr.h"
#include "analysis/loop_info.h"
#include "opt_pass.h"

namespace mocker {

class InductionVariable : public FuncPass {
public:
  InductionVariable(ir::FunctionModule &func, const FuncAttr &funcAttr)
      : FuncPass(func), funcAttr(funcAttr) {}

  bool operator()() override;

private:
  struct IVar {
    IVar() = default;
    IVar(std::shared_ptr<ir::Addr> initial, std::shared_ptr<ir::Addr> step)
        : initial(std::move(initial)), step(std::move(step)) {}

    std::shared_ptr<ir::Addr> initial;
    std::shared_ptr<ir::Addr> step;
  };

  void processLoop(std::size_t header);

  ir::RegMap<IVar> findCandidatePhis(std::size_t header);

  bool isInductionVariable(const std::shared_ptr<ir::ArithBinaryInst> &defInst,
                           const ir::RegSet &curIVs,
                           const ir::RegSet &loopInv) const;

  bool isInductionVariable(const std::shared_ptr<ir::Phi> &defInst,
                           const ir::RegSet &loopInv) const;

  // check whether all uses of the register is in the loop
  bool isLoopVariables(const std::shared_ptr<ir::Reg> &reg,
                       const std::unordered_set<std::size_t> &loopNodes) const {
    const auto &Uses = defUse.getUses(reg);
    for (auto &use : Uses) {
      auto bb = use.getBBLabel();
      if (!isIn(loopNodes, bb))
        return false;
    }
    return true;
  }

private:
  const FuncAttr &funcAttr;
  LoopInfo loopInfo;
  UseDefChain useDef;
  DefUseChain defUse;
  // IV contained by this loop
  std::unordered_map<std::size_t, ir::RegSet> inductionVars;
};

} // namespace mocker

#endif // MOCKER_INDUCTION_VARIABLE_H

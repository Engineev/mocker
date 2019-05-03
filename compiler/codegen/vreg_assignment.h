// Assign each IR Register an NASM virtual register.
// Phi-functions are handled in this phase.

#ifndef MOCKER_VREG_ASSIGNMENT_H
#define MOCKER_VREG_ASSIGNMENT_H

#include <cassert>

#include "ir/module.h"
#include "nasm/module.h"

namespace mocker {

class VRegAssignment {
public:
  void init(const ir::FunctionModule &module);

  void set(const std::shared_ptr<ir::Reg> &irReg,
           const std::shared_ptr<nasm::Register> &vReg) {
    mp[irReg] = vReg;
  }

  const std::shared_ptr<nasm::Register> &
  getVReg(const std::shared_ptr<ir::Reg> &reg) const {
    return mp.at(reg);
  }

private:
  ir::RegMap<std::shared_ptr<nasm::Register>> mp;
};

} // namespace mocker

#endif // MOCKER_VREG_ASSIGNMENT_H

#ifndef MOCKER_NASM_HELPER_H
#define MOCKER_NASM_HELPER_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "addr.h"
#include "inst.h"

namespace mocker {
namespace nasm {

std::vector<std::shared_ptr<Register>>
getInvolvedRegs(const std::shared_ptr<Addr> &addr);

std::vector<std::shared_ptr<Register>>
getInvolvedRegs(const std::shared_ptr<Inst> &inst);

// Get the registers that are used as an operand in [inst]
std::vector<std::shared_ptr<Register>>
getUsedRegs(const std::shared_ptr<Inst> &inst);

std::vector<std::shared_ptr<Register>>
getDefinedRegs(const std::shared_ptr<Inst> &inst);

std::vector<std::shared_ptr<EffectiveAddr>>
getInvolvedMem(const std::shared_ptr<Inst> &inst);

std::shared_ptr<Addr> replaceRegs(const std::shared_ptr<Addr> &addr,
                                  const RegMap<std::shared_ptr<Register>> &mp);

std::shared_ptr<Inst> replaceRegs(const std::shared_ptr<Inst> &inst,
                                  const RegMap<std::shared_ptr<Register>> &mp);

inline bool isVReg(const std::shared_ptr<Register> &reg) {
  return reg->getIdentifier().at(0) == 'v';
}

} // namespace nasm
} // namespace mocker

#endif // MOCKER_NASM_HELPER_H

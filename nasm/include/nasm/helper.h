#ifndef MOCKER_NASM_HELPER_H
#define MOCKER_NASM_HELPER_H

#include <memory>
#include <vector>

#include "addr.h"
#include "inst.h"

namespace mocker {
namespace nasm {

std::vector<std::shared_ptr<Register>>
getInvolvedRegs(const std::shared_ptr<Addr> &addr);

std::vector<std::shared_ptr<Register>>
getInvolvedRegs(const std::shared_ptr<Inst> &inst);

} // namespace nasm
} // namespace mocker

#endif // MOCKER_NASM_HELPER_H

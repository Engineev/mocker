#ifndef MOCKER_CODEGEN_COMMON_H
#define MOCKER_CODEGEN_COMMON_H

#include "ir/ir_inst.h"

namespace mocker {

std::string irGlobalVar2Label(const std::shared_ptr<ir::Reg> &reg) {
  return "L" + reg->getIdentifier();
}

std::string irBBLabel2Label(std::size_t label) {
  return ".L" + std::to_string(label);
}

} // namespace mocker

#endif // MOCKER_CODEGEN_COMMON_H

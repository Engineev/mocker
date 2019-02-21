#ifndef MOCKER_HELPER_H
#define MOCKER_HELPER_H

#include "ir_inst.h"

#include <functional>
#include <string>
#include <vector>

namespace mocker {
namespace ir {

std::shared_ptr<Addr> getDest(const std::shared_ptr<IRInst> &inst);

const std::string &getLocalRegIdentifier(const std::shared_ptr<Addr> &addr);

std::vector<std::shared_ptr<Addr>>
getOperandsUsed(const std::shared_ptr<IRInst> &inst);

std::vector<std::reference_wrapper<std::shared_ptr<Addr>>>
getOperandUsedRef(const std::shared_ptr<IRInst> &inst);

} // namespace ir
} // namespace mocker

#endif // MOCKER_HELPER_H

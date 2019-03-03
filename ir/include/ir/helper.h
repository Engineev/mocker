#ifndef MOCKER_HELPER_H
#define MOCKER_HELPER_H

#include "ir_inst.h"
#include "module.h"

#include <functional>
#include <string>
#include <vector>

namespace mocker {
namespace ir {

template <class T, class V> decltype(auto) dyc(V &&v) {
  return std::dynamic_pointer_cast<T>(v);
}

template <class T, class V> decltype(auto) cdyc(V &&v) {
  return std::dynamic_pointer_cast<const T>(v);
}

std::shared_ptr<const Addr> getDest(const std::shared_ptr<IRInst> &inst);

// std::shared_ptr<Addr> &getDestRef(const std::shared_ptr<IRInst> &inst);

const std::string &
getLocalRegIdentifier(const std::shared_ptr<const Addr> &addr);

std::vector<std::shared_ptr<const Addr>>
getOperandsUsed(const std::shared_ptr<IRInst> &inst);

std::shared_ptr<Addr> copy(const std::shared_ptr<const ir::Addr> &addr);

std::shared_ptr<IRInst> copy(const std::shared_ptr<ir::IRInst> &inst);

std::shared_ptr<IRInst> copyWithReplacedOperands(
    const std::shared_ptr<ir::IRInst> &inst,
    const std::vector<std::shared_ptr<const ir::Addr>> &operands);

std::shared_ptr<IRInst>
copyWithReplacedDest(const std::shared_ptr<ir::IRInst> &inst,
                     const std::shared_ptr<const ir::Addr> &newDest);

// terminate if an error occurs
void verifyFuncModule(const ir::FunctionModule &func);

void verifyModule(const ir::Module &module);

} // namespace ir
} // namespace mocker

#endif // MOCKER_HELPER_H

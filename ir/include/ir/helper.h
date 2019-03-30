#ifndef MOCKER_HELPER_H
#define MOCKER_HELPER_H

#include "ir_inst.h"
#include "module.h"

#include <functional>
#include <string>
#include <vector>

#define MOCKER_IR_DYC(TYPE)                                                    \
  template <class V> std::shared_ptr<TYPE> dyc_impl(V &&v, TYPE *) {           \
    if (v->getInstType() == IRInst::TYPE)                                      \
      return std::static_pointer_cast<TYPE>(v);                                \
    return nullptr;                                                            \
  }

// fast dynamic cast
namespace mocker {
namespace ir {
namespace detail {

template <class T, class V> std::shared_ptr<T> dyc_impl(V &&v, T *) {
  return std::dynamic_pointer_cast<T>(v);
}

MOCKER_IR_DYC(Deleted)
MOCKER_IR_DYC(Comment)
MOCKER_IR_DYC(AttachedComment)
MOCKER_IR_DYC(Assign)
MOCKER_IR_DYC(ArithUnaryInst)
MOCKER_IR_DYC(ArithBinaryInst)
MOCKER_IR_DYC(RelationInst)
MOCKER_IR_DYC(Store)
MOCKER_IR_DYC(Load)
MOCKER_IR_DYC(AllocVar)
MOCKER_IR_DYC(Malloc)
MOCKER_IR_DYC(StrCpy)
MOCKER_IR_DYC(Branch)
MOCKER_IR_DYC(Jump)
MOCKER_IR_DYC(Ret)
MOCKER_IR_DYC(Call)
MOCKER_IR_DYC(Phi)

} // namespace detail

template <class T, class V> std::shared_ptr<T> dyc(V &&v) {
  return detail::dyc_impl(std::forward<V>(v), (T *)(nullptr));
}

std::shared_ptr<Reg> dycLocalReg(const std::shared_ptr<Addr> &addr);

std::shared_ptr<Reg> dycGlobalReg(const std::shared_ptr<Addr> &addr);

} // namespace ir
} // namespace mocker

namespace mocker {
namespace ir {

std::shared_ptr<Reg> getDest(const std::shared_ptr<IRInst> &inst);

std::vector<std::shared_ptr<Addr>>
getOperandsUsed(const std::shared_ptr<IRInst> &inst);

std::shared_ptr<IRInst> copyWithReplacedOperands(
    const std::shared_ptr<ir::IRInst> &inst,
    const std::vector<std::shared_ptr<ir::Addr>> &operands);

std::shared_ptr<IRInst>
copyWithReplacedDest(const std::shared_ptr<ir::IRInst> &inst,
                     const std::shared_ptr<ir::Reg> &newDest);

// terminate if an error occurs
void verifyFuncModule(const ir::FunctionModule &func);

void verifyModule(const ir::Module &module);

} // namespace ir
} // namespace mocker

#endif // MOCKER_HELPER_H

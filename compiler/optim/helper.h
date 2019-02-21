#ifndef MOCKER_OPTIM_HELPER_H
#define MOCKER_OPTIM_HELPER_H

#include <functional>
#include <unordered_map>

#include "ir/helper.h"
#include "ir/ir_inst.h"
#include "ir/module.h"

namespace mocker {

std::unordered_map<std::string, std::shared_ptr<ir::IRInst>>
buildInstDefine(const ir::FunctionModule &func);

std::unordered_map<std::size_t, std::vector<std::size_t>>
buildBlockPredecessors(const ir::FunctionModule &func);

template <class T, class V> decltype(auto) dyc(V &&v) {
  return std::dynamic_pointer_cast<T>(v);
}

void removeInstIf(
    ir::FunctionModule &func,
    std::function<bool(const std::shared_ptr<ir::IRInst> &)> condition);

void removeDeletedInsts(ir::FunctionModule &func);

std::shared_ptr<ir::IRInst> deletePhiOption(const std::shared_ptr<ir::Phi> &phi,
                                            std::size_t bbLabel);

std::shared_ptr<ir::IRInst>
replacePhiOption(const std::shared_ptr<ir::Phi> &phi, std::size_t oldLabel,
                 std::size_t newLabel);

} // namespace mocker

#endif // MOCKER_OPTIM_HELPER_H

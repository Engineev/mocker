#ifndef MOCKER_OPTIM_HELPER_H
#define MOCKER_OPTIM_HELPER_H

#include <unordered_map>

#include "ir/helper.h"
#include "ir/ir_inst.h"
#include "ir/module.h"

namespace mocker {

inline std::unordered_map<std::string, std::shared_ptr<ir::IRInst>>
buildInstDefine(const ir::FunctionModule &func) {
  std::unordered_map<std::string, std::shared_ptr<ir::IRInst>> res;

  for (auto &bb : func.getBBs()) {
    for (auto &phi : bb.getPhis())
      res[std::dynamic_pointer_cast<ir::LocalReg>(phi->dest)->identifier] = phi;
    for (auto &inst : bb.getInsts()) {
      auto dest = ir::getDest(inst);
      if (!dest)
        continue;
      auto p = std::dynamic_pointer_cast<ir::LocalReg>(dest);
      assert(p);
      res[p->identifier] = inst;
    }
  }

  return res;
}

template <class T, class V> decltype(auto) dyc(V &&v) {
  return std::dynamic_pointer_cast<T>(v);
}

} // namespace mocker

#endif // MOCKER_OPTIM_HELPER_H

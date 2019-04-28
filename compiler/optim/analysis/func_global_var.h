#ifndef MOCKER_FUNC_GLOBAL_VAR_H
#define MOCKER_FUNC_GLOBAL_VAR_H

#include "optim/opt_pass.h"

#include <unordered_map>
#include <unordered_set>

namespace mocker {

class FuncGlobalVar {
public:
  void init(const ir::Module &module);

  std::unordered_set<std::string>
  getInvolved(const std::string &funcName) const;

  const std::unordered_set<std::string> &
  getUse(const std::string &funcName) const {
    return use.at(funcName);
  }

  const std::unordered_set<std::string> &
  getDef(const std::string &funcName) const {
    return def.at(funcName);
  }

private:
  std::unordered_map<std::string, std::unordered_set<std::string>> use, def;
};

} // namespace mocker

#endif // MOCKER_FUNC_GLOBAL_VAR_H

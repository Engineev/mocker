#ifndef MOCKER_FUNC_ATTR_H
#define MOCKER_FUNC_ATTR_H

#include "optim/opt_pass.h"

#include <unordered_map>
#include <unordered_set>
#include <string>

#include "set_operation.h"

namespace mocker {

class FuncAttr {
public:
  void init(const ir::Module &module);

  std::unordered_set<std::string>
  getInvolvedGlobalVars(const std::string &funcName) const;

  const std::unordered_set<std::string> &
  getGlobalVarUses(const std::string &funcName) const {
    return globalVarUses.at(funcName);
  }

  const std::unordered_set<std::string> &
  getGlobalVarDefs(const std::string &funcName) const {
    return globalVarDefs.at(funcName);
  }

  bool isPure(const std::string & funcName) const {
    return isIn(pureFuncs, funcName);
  }

private:
  void buildGlobalVarInfo(const ir::Module & module);

  // conservative
  bool isPureFunc(const ir::FunctionModule & func);

private:
  std::unordered_map<std::string, std::unordered_set<std::string>>
    globalVarUses, globalVarDefs;

  std::unordered_set<std::string> pureFuncs;
};

} // namespace mocker

#endif //MOCKER_FUNC_ATTR_H
